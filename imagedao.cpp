#include "imagedao.h"
#include "sqlite3.h"
#include "dct/fast-dct-lee.h"
#include "sqlitehelper.h"

#include <set>
#include <unordered_set>
#include <unordered_map>

#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QThread>
#include <QElapsedTimer>
#include <QDebug>
#include <QFile>
#include <QPainter>
#include <QClipboard>
#include <QGuiApplication>
#include <QDataStream>
#include <QThreadPool>
#include <QCryptographicHash>

ImageDao *ImageDao::m_instance;

#define CHECK(expression) \
{ \
  if((expression) != SQLITE_OK) { \
    qWarning("%s:%d SQLite: %s", __FUNCTION__, __LINE__, sqlite3_errmsg(m_db)); \
    goto error; \
  } \
}

#define CHECK_EXEC(sql) \
{ \
  char *error; \
  if(sqlite3_exec(m_db, sql, nullptr, nullptr, &error) != SQLITE_OK) { \
    qWarning("%s:%d SQLite: %s", __FUNCTION__, __LINE__, error); \
    sqlite3_free(error); \
    goto error; \
  } \
}

#define CHECK_STEP(stmt) \
{ \
  if(sqlite3_step(stmt) != SQLITE_DONE) { \
    qWarning("%s:%d SQLite: %s", __FUNCTION__, __LINE__, sqlite3_errmsg(m_db)); \
    goto error; \
  } \
}

#define STEP_LOOP_BEGIN(stmt) \
{ \
  int rval; \
  while((rval = sqlite3_step(stmt)) == SQLITE_ROW) {

#define STEP_LOOP_END \
  } \
  if(rval != SQLITE_DONE) { \
    qWarning("%s:%d SQLite: %s", __FUNCTION__, __LINE__, sqlite3_errmsg(m_db)); \
    goto error; \
  } \
}

#define STEP_SINGLE(stmt) \
{ \
  if(sqlite3_step(stmt) != SQLITE_ROW) { \
    qWarning("%s:%d SQLite: %s", __FUNCTION__, __LINE__, sqlite3_errmsg(m_db)); \
    goto error; \
  } \
}

ImageDao::ImageDao(QObject *parent) :
  QObject(parent),
  m_connPool(QStringLiteral("main.db"), SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
{
  m_conn = m_connPool.open();
  m_db = m_conn->m_db;

  auto idfw = new ImageDaoDeferredWriter(m_connPool.open());
  connect(this, &ImageDao::deferredSync, idfw, &ImageDaoDeferredWriter::sync);
  connect(this, &ImageDao::deferredAddTag, idfw, &ImageDaoDeferredWriter::addTag);
  connect(this, &ImageDao::deferredRemoveTag, idfw, &ImageDaoDeferredWriter::removeTag);
  connect(this, &ImageDao::deferredUpdateDeleted, idfw, &ImageDaoDeferredWriter::updateDeleted);
  connect(this, &ImageDao::deferredWriteImage, idfw, &ImageDaoDeferredWriter::writeImage);
  connect(idfw, &ImageDaoDeferredWriter::writeComplete, this, &ImageDao::writeComplete);

  connect(&m_writeThread, &QThread::finished, idfw, &QObject::deleteLater);
  idfw->moveToThread(&m_writeThread);
  m_writeThread.start();

  QVariant version;

  CHECK_EXEC("PRAGMA journal_mode = WAL");

  bool createNewDatabase = !tableExists(QStringLiteral("store"));

  if(createNewDatabase) {
    CHECK_EXEC("CREATE TABLE store (id INTEGER PRIMARY KEY, hash TEXT UNIQUE, date INTEGER, image BLOB)");
    CHECK_EXEC("CREATE TABLE tag (id INTEGER, tag TEXT, PRIMARY KEY (id, tag))");
  }

  if(!tableExists("meta")) {
    version = 0;
  } else {
    version = metaGet(QStringLiteral("version"));
  }

  qInfo("Database Version: %d", version.toInt());

  if(version == 0) {
    qInfo("Upgrading database format from 0 to 1");
    CHECK_EXEC("CREATE TABLE meta (key TEXT PRIMARY KEY, type TEXT, value TEXT)");
    metaPut(QStringLiteral("version"), version = 1);
  }

  if(version == 1) {
    qInfo("Upgrading database format from 1 to 2");
    CHECK_EXEC("ALTER TABLE store ADD COLUMN width INTEGER");
    CHECK_EXEC("ALTER TABLE store ADD COLUMN height INTEGER");
    CHECK_EXEC("ALTER TABLE store ADD COLUMN origin_url TEXT");
    metaPut(QStringLiteral("version"), version = 2);
  }

  if(version == 2) {
    qInfo("Upgrading database format from 2 to 3");
    CHECK_EXEC("ALTER TABLE store ADD COLUMN phash INTEGER");
    metaPut(QStringLiteral("version"), version = 3);
  }

  if(version == 3) {
    qInfo("Upgrading database format from 3 to 4");

    CHECK_EXEC("BEGIN TRANSACTION");

    CHECK_EXEC("CREATE TABLE new_store (id INTEGER PRIMARY KEY, hash TEXT UNIQUE, image BLOB)");
    CHECK_EXEC("CREATE TABLE image (id INTEGER PRIMARY KEY, date INTEGER, width INTEGER, height INTEGER, phash INTEGER, origin_url TEXT)");
    CHECK_EXEC("INSERT INTO new_store SELECT id, hash, image FROM store");
    CHECK_EXEC("INSERT INTO image SELECT id, date, width, height, phash, origin_url FROM store");
    CHECK_EXEC("DROP TABLE store");
    CHECK_EXEC("ALTER TABLE new_store RENAME TO store");

    metaPut(QStringLiteral("version"), version = 4);

    CHECK_EXEC("END TRANSACTION");
  }

  if(version == 4) {
    qInfo("Upgrading database format from 4 to 5");
    CHECK_EXEC("ALTER TABLE image ADD COLUMN deleted INTEGER");

    metaPut(QStringLiteral("version"), version = 5);
  }

  createTemporaryTable("search", {});

  m_ps_addTag.init(m_db, "INSERT OR IGNORE INTO tag (id, tag) VALUES (?1, ?2)");

  m_ps_removeTag.init(m_db, "DELETE FROM tag WHERE id = ?1 AND tag = ?2");
  m_ps_tagsById.init(m_db, "SELECT tag FROM tag WHERE id = ?1");
  m_ps_idByHash.init(m_db, "SELECT id FROM store WHERE hash = ?1");

  m_ps_transStart.init(m_db, "BEGIN TRANSACTION");
  m_ps_transEnd.init(m_db, "END TRANSACTION");
error:
  return;
}

ImageDao::~ImageDao()
{
  m_writeThread.quit();
  m_writeThread.wait();
  m_connPool.close(m_conn);
  qInfo(__FUNCTION__);
}

bool ImageDao::tableExists(const QString &table)
{
  SQLitePreparedStatement ps;
  ps.init(m_db, "SELECT name FROM sqlite_master WHERE type='table' AND name=?1");
  ps.bind(1, table);
  return ps.step();
}

void ImageDao::metaPut(const QString &key, const QVariant &val)
{
  lockWrite();

  SQLitePreparedStatement ps(m_conn, "INSERT OR REPLACE INTO meta (key, type, value) VALUES (?1, ?2, ?3)");
  ps.bind(1, key);

  sqlite3_bind_text(ps.m_stmt, 2, val.typeName(), -1, nullptr);
  if(val.type() == QVariant::Int) {
    ps.bind(3, val.toInt());
  } else if(val.type() == QVariant::String) {
    ps.bind(3, val.toString());
  } else {
    QByteArray data;
    QDataStream dsw(&data, QIODevice::WriteOnly);
    dsw << val;
    ps.bind(3, data);
  }
  ps.step(__FUNCTION__);

  unlockWrite();
}

QVariant ImageDao::metaGet(const QString &key)
{
  QVariant result;

  SQLitePreparedStatement ps(m_conn, "SELECT type, value FROM meta WHERE key = ?1");
  ps.bind(1, key);
  if(ps.step(__FUNCTION__)) {
    const char *typeName = (const char *)sqlite3_column_text(ps.m_stmt, 0);
    QVariant::Type type = QVariant::nameToType(typeName);
    if(type == QVariant::Int) {
      result = ps.resultInteger(1);
    } else if(type == QVariant::String) {
      result = ps.resultString(1);
    } else {
      const char *data = (const char *)sqlite3_column_blob(ps.m_stmt, 1);
      int bytes = sqlite3_column_bytes(ps.m_stmt, 1);

      QByteArray raw = QByteArray::fromRawData(data, bytes);
      QDataStream dsr(&raw, QIODevice::ReadOnly);
      dsr >> result;
    }
  }

  return result;
}

QList<QObject *> ImageDao::addTag(const QList<QObject *> &irefs, const QString &tag)
{
  QList<QObject *> result;

  for(QObject *obj : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(obj)) {
      if(!iref->m_tags.contains(tag)) {
        iref->m_tags.insert(tag);
        emit iref->tagsChanged();

        result.append(iref);
      }
    }
  }

  emit deferredAddTag(result, tag);

  return result;
}

QList<QObject *> ImageDao::removeTag(const QList<QObject *> &irefs, const QString &tag)
{
  QList<QObject *> result;

  for(QObject *obj : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(obj)) {
      if(iref->m_tags.contains(tag)) {
        iref->m_tags.remove(tag);
        emit iref->tagsChanged();

        result.append(iref);
      }
    }
  }

  emit deferredRemoveTag(result, tag);

  return result;
}

static const uint64_t m1 = 0x5555555555555555; //binary: 0101...
static const uint64_t m2 = 0x3333333333333333; //binary: 00110011..
static const uint64_t m4 = 0x0f0f0f0f0f0f0f0f; //binary:  4 zeros,  4 ones ...

static int popcount64b(uint64_t x) {
  x -= (x >> 1) & m1;             //put count of each 2 bits into those 2 bits
  x = (x & m2) + ((x >> 2) & m2); //put count of each 4 bits into those 4 bits
  x = (x + (x >> 4)) & m4;        //put count of each 8 bits into those 8 bits
  x += x >>  8;  //put count of each 16 bits into their lowest 8 bits
  x += x >> 16;  //put count of each 32 bits into their lowest 8 bits
  x += x >> 32;  //put count of each 64 bits into their lowest 8 bits
  return x & 0x7f;
}


static int hammingDistance(uint64_t a, uint64_t b) {
  return popcount64b(a ^ b);
}


static void findMatchesLinear(const std::vector<uint64_t> &hashes, std::set<uint64_t> &result, int maxd) {
  auto iter_end = hashes.end();
  for(auto i = hashes.begin(); i != iter_end; ++i) {
    bool found_match = false;
    for(auto j = i + 1; j != iter_end; ++j) {
      if(hammingDistance(*i, *j) <= maxd) {
        result.insert(*j);
        found_match = true;
      }
    }
    if(found_match) {
      result.insert(*i);
    }
  }
}

// Each hash belongs to at most one cluster.
using HashToCluster = std::unordered_map<uint64_t, int>;
// Each cluster consists of a number of (unique) hashes.
using ClusterToHashList = std::unordered_map<int, std::vector<uint64_t>>;

static void findClusters(const std::vector<uint64_t> &hashes, HashToCluster &hashToCluster, ClusterToHashList &clusters, int &nextClusterId, int maxd) {
  auto iter_end = hashes.end();
  for(auto i = hashes.begin(); i != iter_end; ++i) {
    uint64_t hash_i = *i;
    for(auto j = i + 1; j != iter_end; ++j) {
      uint64_t hash_j = *j;
      int distance = hammingDistance(hash_i, hash_j);
      if(distance <= maxd) {
        // found a pair
        auto icluster = hashToCluster.find(hash_i);
        auto jcluster = hashToCluster.find(hash_j);

        auto End = hashToCluster.end();

        if(icluster == End && jcluster == End) {
          // both don't belong to a cluster
          int id = nextClusterId++;

          clusters[id].push_back(hash_i);
          clusters[id].push_back(hash_j);

          hashToCluster[hash_i] = id;
          hashToCluster[hash_j] = id;

          qDebug() << "create new cluster" << id << "hashi" << hash_i << "hash_j" << hash_j << "dist" << distance;
        } else if(icluster == End && jcluster != End) {
          // j already belongs to a cluster
          int id = jcluster->second;
          clusters[id].push_back(hash_i);
          hashToCluster[hash_i] = id;

          qDebug() << "merge into j cluster" << id << "hashi" << hash_i << "hash_j" << hash_j << "dist" << distance;
        } else if(icluster != End && jcluster == End) {
          // i already belongs to a cluster
          int id = icluster->second;
          clusters[id].push_back(hash_j);
          hashToCluster[hash_j] = id;

          qDebug() << "merge into i cluster" << id << "hashi" << hash_i << "hash_j" << hash_j << "dist" << distance;
        } else if(icluster->second != jcluster->second) {
          // both belong to different clusters
          int idi = icluster->second;
          int idj = jcluster->second;

          // merge jcluster with icluster
          auto &icluster_data = clusters[idi];
          const auto &jcluster_data = clusters[idj];

          for(auto h : jcluster_data) {
            icluster_data.push_back(h);
            hashToCluster[h] = idi;
          }

          // delete jcluster
          clusters.erase(idj);

          qDebug() << "merge clusters" << idi << "and" << idj << "hashi" << hash_i << "hash_j" << hash_j << "dist" << distance;
        } else {
          // both belong to the same cluster already, nothing to do
        }
      }
    }
  }
}

QList<QObject *> ImageDao::findAllDuplicates(const QList<QObject *> &irefs, int maxDistance)
{
  std::vector<uint64_t> hashList;
  std::unordered_map<uint64_t, std::vector<ImageRef *>> irefLookup;

  ClusterToHashList clusterToHashList;
  HashToCluster hashToCluster;
  int nextClusterId = 0;

  QElapsedTimer timer;
  timer.start();

  for(auto obj : irefs) {
    ImageRef *iref = qobject_cast<ImageRef *>(obj);
    if(iref != nullptr) {
      uint64_t hash = iref->m_phash;

      auto irefIter = irefLookup.find(hash);
      if(irefIter != irefLookup.end()) {
        // hash already exists
        auto cIter = hashToCluster.find(hash);
        if(cIter == hashToCluster.end()) {
          // Hash was seen once before, but a cluster doesn't yet exist.
          int id = nextClusterId++;
          hashToCluster[hash] = id;
          clusterToHashList[id].push_back(hash);
          qDebug() << "create new cluster" << id << "for hash" << hash;
        }

        irefIter->second.push_back(iref);
      } else {
        // new hash
        hashList.push_back(hash);
        irefLookup.emplace(hash, std::vector<ImageRef *>{ iref });
      }
    }
  }

  qDebug() << "pre-existing clusters" << nextClusterId;

  findClusters(hashList, hashToCluster, clusterToHashList, nextClusterId, maxDistance);

  QList<QObject *> output;

  // for each cluster
  for(const auto &p : clusterToHashList) {
    qDebug() << "Cluster Id" << p.first;
    // for each hash
    for(uint64_t h : p.second) {
      // for each iref
      qDebug() << "  Hash" << h;
      for(auto irefptr : irefLookup[h]) {

        qDebug() << "    Iref" << irefptr->m_fileId;
        output.push_back(irefptr);
      }
    }
  }

  qDebug() <<  __FUNCTION__ << "Time:" << timer.elapsed() << "ms Number of results:" << output.size();

  return output;
}

QString ImageDao::hashById(qint64 id)
{
  SQLitePreparedStatement ps(m_conn, "SELECT hash FROM store WHERE id = ?1");
  ps.bind(1, id);
  ps.step(__FUNCTION__);
  return ps.resultString(0);
}

QVariantList ImageDao::tagCount(const QList<QObject *> &irefs) {
  QMap<QString, int> result;
  for(QObject *qobj : irefs) {
    ImageRef *iref = qobject_cast<ImageRef *>(qobj);
    if(iref != nullptr) {
      for(const QString &tag : iref->m_tags) {
        result[tag]++;
      }
    }
  }

  QVariantList out;
  auto iter_end = result.constKeyValueEnd();
  for(auto iter = result.constKeyValueBegin(); iter != iter_end; ++iter) {
    QVariantList r = { (*iter).first, (*iter).second };
    out.append(QVariant::fromValue(r));
  }

  /*
  std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
    return a.toList().at(1) > b.toList().at(1);
  });*/

  return out;
}

QList<QObject *> ImageDao::search(const QList<QObject *> &irefs, const QStringList &tags) {
  QSet<QString> searchTags = QSet<QString>::fromList(tags);

  QList<QObject *> result;
  for(QObject *irobj : irefs) {
    ImageRef *ir = qobject_cast<ImageRef *>(irobj);
    if(ir && ir->m_tags.contains(searchTags)) {
      result.append(ir);
    }
  }

  return result;
}

QList<QObject *> ImageDao::all(bool includeDeleted)
{
  QElapsedTimer timer;
  timer.start();

  QList<QObject *> result;

  SQLitePreparedStatement m_ps_all(m_conn,
    "SELECT image.id, group_concat(tag, ' '), width, height, phash, deleted "
    "FROM image LEFT JOIN tag ON (tag.id = image.id) "
    "WHERE image.deleted IS NULL OR image.deleted <= ?1"
    "GROUP BY image.id "
    "ORDER BY date ASC");

  m_ps_all.bind(1, (qint64)includeDeleted);

  while(m_ps_all.step(__FUNCTION__)) {
    ImageRef *ir = new ImageRef();
    ir->m_fileId = m_ps_all.resultInteger(0);
    ir->m_tags = QSet<QString>::fromList(m_ps_all.resultString(1).split(' ', QString::SkipEmptyParts));
    ir->m_size = { (int)m_ps_all.resultInteger(2), (int)m_ps_all.resultInteger(3) };
    ir->m_phash = m_ps_all.resultInteger(4);
    ir->m_deleted = m_ps_all.resultInteger(5);
    result.append(ir);
  }

  qDebug() << __FUNCTION__ << timer.elapsed() << "ms";

  return result;
}

void ImageDao::addTag(qint64 id, const QString &tag)
{
  m_ps_addTag.bind(1, id);
  m_ps_addTag.bind(2, tag);
  m_ps_addTag.step(__FUNCTION__);
  m_ps_addTag.reset();
}

void ImageDao::removeTag(qint64 id, const QString &tag)
{
  m_ps_removeTag.bind(1, id);
  m_ps_removeTag.bind(2, tag);
  m_ps_removeTag.step(__FUNCTION__);
  m_ps_removeTag.reset();
}

ImageRef *ImageDao::findHash(const QString &hash)
{
  m_ps_idByHash.bind(1, hash);
  if(!m_ps_idByHash.step(__FUNCTION__)) {
    m_ps_idByHash.reset();
    return nullptr;
  }

  qint64 id = m_ps_idByHash.resultInteger(0);

  m_ps_idByHash.reset();

  ImageRef *iref = new ImageRef();

  iref->m_tags = QSet<QString>::fromList(tagsById(id));
  iref->m_fileId = id;
  iref->m_selected = false;

  return iref;
}

void ImageDao::purgeDeletedImages()
{
  lockWrite();
  SQLitePreparedStatement ps_del_store(m_conn, "DELETE FROM store WHERE id IN (SELECT id FROM image WHERE deleted = 1)");
  SQLitePreparedStatement ps_del_tags(m_conn, "DELETE FROM tag WHERE id IN (SELECT id FROM image WHERE deleted = 1)");
  SQLitePreparedStatement ps_del_images(m_conn, "DELETE FROM image WHERE deleted = 1");
  transactionStart();
  ps_del_store.exec();
  ps_del_tags.exec();
  ps_del_images.exec();
  qInfo("Purged %d images from the database", sqlite3_changes(m_db));
  transactionEnd();
  unlockWrite();
}

QList<QObject *> ImageDao::updateDeleted(const QList<QObject *> &irefs, bool deletedValue)
{
  QList<QObject *> result;

  for(QObject *ptr : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(ptr)) {
      if(iref->m_deleted == deletedValue)
        continue;

      iref->m_deleted = deletedValue;
      emit iref->deletedChanged();
      result.append(iref);
    }
  }

  emit deferredUpdateDeleted(result, deletedValue);
  return result;
}


void ImageDao::renderImages(const QList<QObject *> &irefs, const QString &path, int requestedSize, int flags)
{
  QSize reqSize(requestedSize, requestedSize);

  QStringList clipBoardData;

  for(QObject *rptr : irefs) {
    ImageRef *ref = qobject_cast<ImageRef *>(rptr);

    ImageDataContext idc;
    imageDataAcquire(idc, ref->m_fileId);

    QBuffer buffer(&idc.data);
    QImageReader reader(&buffer);
    QByteArray format = reader.format();

    QString basename = QStringLiteral("%1_%2").arg(ref->m_tags.toList().join('_')).arg(ref->m_fileId);
    clipBoardData.append(basename);

    QString filename = QStringLiteral("%1%2").arg(path).arg(basename);

    if(reqSize.isValid()) {
      QImage image = reader.read();
      image = image.scaled(reqSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

      if(flags & PAD_TO_FIT) {
        QImage surface(reqSize, QImage::Format_RGB32);
        surface.fill(Qt::black);
        QPainter painter(&surface);
        painter.drawImage((surface.width() - image.width()) / 2, (surface.height() - image.height()) / 2, image);
        image = surface;
      }

      filename += QStringLiteral(".jpg");
      image.save(filename, "jpg", 100);
    } else {
      filename += QString::asprintf(".%s", format.constData());
      QFile file(filename);
      if(file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(idc.data);
      }
    }

    qInfo() << __FUNCTION__ << "Rendering image:" << filename;

    imageDataRelease(idc);
  }

  if(flags & FNAME_TO_CLIPBOARD) {
    QClipboard *cb = QGuiApplication::clipboard();
    cb->setText(clipBoardData.join('\n'));
  }
}

void ImageDao::transactionStart()
{
  m_ps_transStart.exec();
}

void ImageDao::transactionEnd()
{
  m_ps_transEnd.exec();
}

void ImageDao::lockWrite()
{
  m_writeLock.lock();
}

void ImageDao::unlockWrite()
{
  m_writeLock.unlock();
}

class FixImageMetaDataTask : public QRunnable {
  ImageProcessStatus *status;

public:
  FixImageMetaDataTask(ImageProcessStatus *status) : status(status) { }

  uint64_t perceptualHash(const QImage &image) {
    Q_ASSERT(image.width() == 32);
    Q_ASSERT(image.height() == 32);

    double mat[32][32];

    for(int y = 0; y < 32; y++) {
      const uchar *scanLine = image.constScanLine(y);
      for(int x = 0; x < 32; x++) {
        mat[y][x] = (double)scanLine[x];
      }
    }

    // Calculate 2D DCT type II.
    // Note that we don't care about the scaling, as we're going to compare to the mean anyway.

    // Transform each row
    for(int y = 0; y < 32; y++) {
      FastDctLee_transform(mat[y], 32);
    }

    // Transform each column (we only care about the first 8 columns).
    for(int x = 0; x < 8; x++) {
      double col[32];
      for(int y = 0; y < 32; y++) {
        col[y] = mat[y][x];
      }
      FastDctLee_transform(col, 32);
      for(int y = 0; y < 32; y++) {
        mat[y][x] = col[y];
      }
    }

    // Calculate the mean of each cosine amplitude, but exclude the DC component.
    // The lowest 8x8 frequencies are taken.
    double acc = 0;
    for(int i = 1; i < 65; i++) {
      int y = i / 8;
      int x = i % 8;
      acc += mat[y][x];
    }
    double mean = acc / 64;

    uint64_t hash = 0;
    for(int i = 1; i < 65; i++) {
      int y = i / 8;
      int x = i % 8;

      hash <<= 1;
      hash |= mat[y][x] > mean;
    }

    return hash;
  }

  uint64_t blockHash(const QImage &image) {
    Q_ASSERT(image.width() == 8);
    Q_ASSERT(image.height() == 8);

    quint8 mean[64];

    for(int y = 0; y < 8; y++) {
      const uchar *scanLine = image.constScanLine(y);
      for(int x = 0; x < 8; x++) {
        mean[y * 8 + x] = scanLine[x];
      }
    }

    std::sort(mean, mean + 64);
    int median = (mean[31] + mean[32]) / 2;

    quint64 phash = 0;
    for(int y = 0; y < 8; y++) {
      const uchar *scanLine = image.constScanLine(y);
      for(int x = 0; x < 8; x++) {
        phash <<= 1;
        phash |= scanLine[x] > median;
      }
    }

    return phash;
  }

  uint64_t differenceHash(const QImage &image) {
    uint64_t hash = 0;

    Q_ASSERT(image.width() == 9);
    Q_ASSERT(image.height() == 8);

    for(int y = 0; y < 8; y++) {
      const uchar *scanLine = image.constScanLine(y);
      for(int x = 0; x < 8; x++) {
        hash <<= 1;
        hash |= scanLine[x] < scanLine[x + 1];
      }
    }

    return hash;
  }

  QImage autoCrop(const QImage &image, int threshold) {
    int baseColor = image.constScanLine(0)[0];

    int h = image.height();
    int w = image.width();

    int top = 0;
    int left = w;

    int bottom = h;
    int right = 0;

    int y = 0;
    while(y < h && left > 0) {
      const uint8_t *scanLine = image.constScanLine(y);
      int x;
      for(x = 0; x < left; x++) {
        if(std::abs((scanLine[x] - baseColor)) > threshold) {
          break;
        }
      }

      if(x == w) {
        top = y;
      } else if(x < left) {
        // continue
        left = x;
      } else {

      }
      y++;
    }

    y = h - 1;
    while(y >= 0 && right < w) {
      const uint8_t *scanLine = image.constScanLine(y);
      int x;
      for(x = w - 1; x >= right; x--) {
        if(std::abs((scanLine[x] - baseColor)) > threshold) {
          break;
        }
      }

      if(x == -1) {
        bottom = y;
      } else if(x >= right) {
        right = x + 1;
      } else {
        // continue
      }
      y--;
    }

    if(bottom <= top) {
      // Blank image
      left = 0;
      top = 0;
      right = 1;
      bottom = 1;
    }

    if(w != right || h != bottom || left != 0 || top != 0) {
      return image.copy(left, top, right - left, bottom - top);
    }

    return image;
  }

  void run() override {
    ImageDao *dao = ImageDao::instance();
    SQLiteConnection *conn = dao->connPool()->open();

    conn->exec("BEGIN TRANSACTION", __FUNCTION__);
    {
      SQLitePreparedStatement ps_update(conn, "UPDATE image SET width = ?1, height = ?2, phash = ?3 WHERE id = ?4");
      SQLitePreparedStatement ps(conn, "SELECT id FROM image ORDER BY id");
      qreal progress = 0.0;
      while(ps.step(__FUNCTION__)) {
        qint64 id = ps.resultInteger(0);

        ImageDao::ImageDataContext idc;
        dao->imageDataAcquire(idc, id);
        QBuffer buffer(&idc.data);
        QImageReader reader(&buffer);

        QSize size = reader.size();

        reader.setBackgroundColor(Qt::darkGray);
        reader.setScaledSize({128, 128});
        //reader.setScaledSize({9, 8});

        QImage image = reader.read();
        if(!image.isNull()) {
          image.convertTo(QImage::Format_Grayscale8);
          //image.save(QStringLiteral("%1_crop_input.png").arg(id));
          image = autoCrop(image, 10);
          //image.save(QStringLiteral("%1_crop_output.png").arg(id));
          image = image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
          image.convertTo(QImage::Format_Grayscale8);
          //image.save(QStringLiteral("%1_phash_input.png").arg(id));
          uint64_t phash = perceptualHash(image);
          //qDebug() << "id" << id << "phash" << phash;

          ps_update.bind(1, size.width());
          ps_update.bind(2, size.height());
          ps_update.bind(3, phash);
          ps_update.bind(4, id);
          ps_update.exec(__FUNCTION__);
        }

        dao->imageDataRelease(idc);

        emit status->update({}, ++progress);
      }
      emit status->complete();
    }

    conn->exec("END TRANSACTION", __FUNCTION__);

    dao->connPool()->close(conn);
  }
};

void ImageDao::fixImageMetaData(ImageProcessStatus *status)
{
  QThreadPool::globalInstance()->start(new FixImageMetaDataTask(status));
}

QStringList ImageDao::tagsById(qint64 id)
{
  QStringList tags;

  m_ps_tagsById.bind(1, id);
  while(m_ps_tagsById.step()) {
    tags.append(m_ps_tagsById.resultString(0));
  }
  m_ps_tagsById.reset();

  return tags;
}


void ImageDao::imageDataAcquire(ImageDao::ImageDataContext &idc, qint64 id)
{
  idc.conn = m_connPool.open();
  idc.ps.init(idc.conn->m_db, "SELECT image FROM store WHERE id = ?1");
  idc.ps.bind(1, id);
  idc.ps.step(__FUNCTION__);

  const char *data = (const char *)sqlite3_column_blob(idc.ps.m_stmt, 0);
  int bytes = sqlite3_column_bytes(idc.ps.m_stmt, 0);

  idc.data = QByteArray::fromRawData(data, bytes);
}

void ImageDao::imageDataRelease(ImageDao::ImageDataContext &idc)
{
  idc.ps.reset();
  idc.ps.destroy();

  m_connPool.close(idc.conn);
}

QImage ImageDao::requestImage(qint64 id, const QSize &requestedSize, volatile bool *cancelled)
{
  QImage result;

  ImageDataContext idc;
  imageDataAcquire(idc, id);

  if(!(*cancelled)) {
    QBuffer buffer(&idc.data);
    QImageReader reader(&buffer);

    if(requestedSize.isValid()) {
      auto actualSize = reader.size();

      float scalex = (float)requestedSize.width() / actualSize.width();
      float scaley = (float)requestedSize.height() / actualSize.height();

      QSize newSize;
      if(scalex > scaley) {
        newSize.setWidth(requestedSize.width());
        newSize.setHeight(actualSize.height() * scalex);
      } else {
        newSize.setWidth(actualSize.width() * scaley);
        newSize.setHeight(requestedSize.height());
      }

      reader.setScaledSize(newSize);
    }

    result = reader.read();
  }

  imageDataRelease(idc);
  return result;
}

ImageDao *ImageDao::instance()
{
  if(!m_instance) {
    m_instance = new ImageDao();
  }

  return m_instance;
}

void ImageDao::createTemporaryTable(const QString &tableName, const QStringList &items)
{
  sqlite3_stmt *stmt = nullptr;
  CHECK_EXEC(qUtf8Printable(QStringLiteral("CREATE TEMPORARY TABLE IF NOT EXISTS %1 (item PRIMARY KEY);"
                                           "DELETE FROM %1").arg(tableName)));

  CHECK(sqlite3_prepare_v2(m_db, qUtf8Printable(QStringLiteral("INSERT INTO %1 (item) VALUES (?1)").arg(tableName)), -1, &stmt, nullptr));
  for(const QString &id : items) {
    CHECK(sqlite3_bind_text(stmt, 1, qUtf8Printable(id), -1, SQLITE_TRANSIENT));
    CHECK_STEP(stmt);
    CHECK(sqlite3_reset(stmt));
  }

error:
  sqlite3_finalize(stmt);
}

QStringList ImageRef::tags()
{
  return m_tags.toList();
}

void ImageDaoDeferredWriter::startTransaction()
{
  qDebug() << __FUNCTION__;
  SQLitePreparedStatement ps(m_conn, "BEGIN TRANSACTION");
  ps.exec(__FUNCTION__);
}

void ImageDaoDeferredWriter::endTransaction()
{
  qDebug() << __FUNCTION__;
  SQLitePreparedStatement ps(m_conn, "END TRANSACTION");
  ps.exec(__FUNCTION__);
}

void ImageDaoDeferredWriter::startWrite() {
  if(!m_inTransaction) {
    ImageDao::instance()->lockWrite();
    startTransaction();
    m_inTransaction = true;
    QTimer::singleShot(0, this, &ImageDaoDeferredWriter::endWrite);
  }
}

ImageDaoDeferredWriter::ImageDaoDeferredWriter(SQLiteConnection *conn, QObject *parent) : m_conn(conn), QObject(parent)
{

}

void ImageDaoDeferredWriter::endWrite() {
  if(m_inTransaction) {
    endTransaction();
    m_inTransaction = false;
    ImageDao::instance()->unlockWrite();
  }
}

void ImageDaoDeferredWriter::addTag(const QList<QObject *> &irefs, const QString &tag)
{
  startWrite();
  SQLitePreparedStatement ps(m_conn, "INSERT OR IGNORE INTO tag (id, tag) VALUES (?1, ?2)");
  for(QObject *qobj : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(qobj)) {
      ps.bind(1, iref->m_fileId);
      ps.bind(2, tag);
      ps.exec(__FUNCTION__);
    }
  }
}

void ImageDaoDeferredWriter::removeTag(const QList<QObject *> &irefs, const QString &tag)
{
  startWrite();
  SQLitePreparedStatement ps(m_conn, "DELETE FROM tag WHERE id = ?1 AND tag = ?2");
  for(QObject *qobj : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(qobj)) {
      ps.bind(1, iref->m_fileId);
      ps.bind(2, tag);
      ps.exec(__FUNCTION__);
    }
  }
}

void ImageDaoDeferredWriter::updateDeleted(const QList<QObject *> &irefs, bool deleted)
{
  startWrite();
  SQLitePreparedStatement ps(m_conn, "UPDATE image SET deleted = ?1 WHERE id = ?2");
  for(QObject *ptr : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(ptr)) {
      ps.bind(1, (qint64)deleted);
      ps.bind(2, iref->m_fileId);
      ps.exec(__FUNCTION__);
    }
  }
}

void ImageDaoDeferredWriter::sync(ImageDaoSyncPoint *syncPoint, const QVariant &userData) {
  endWrite();
  emit syncPoint->sync(userData);
}

void ImageDaoDeferredWriter::writeImage(const QUrl &url, const QByteArray &data)
{
  startWrite();

  SQLitePreparedStatement ps(m_conn, "INSERT OR IGNORE INTO store (hash, image) VALUES (?1, ?2)");
  SQLitePreparedStatement ps_id_by_hash(m_conn, "SELECT id FROM store WHERE hash = ?1");
  SQLitePreparedStatement ps_image(m_conn, "INSERT INTO image (id, date) VALUES (?1, datetime())");

  QByteArray hashBytes = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
  QString hash = hashBytes.toHex();
  ps.bind(1, hash);
  ps.bind(2, data);
  ps.exec(__FUNCTION__);

  if(sqlite3_changes(m_conn->m_db) == 0) {
    qWarning("Duplicate image not inserted");
    return;
  }

  qint64 last_id = sqlite3_last_insert_rowid(m_conn->m_db);
  qInfo("Inserted image with ID %lld", last_id);

  ps_image.bind(1, last_id);
  ps_image.exec(__FUNCTION__);

  endWrite();

  emit writeComplete(url, hash);
}
