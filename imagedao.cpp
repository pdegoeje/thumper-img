#include "imagedao.h"
#include "sqlite3.h"
#include "sqlitehelper.h"
#include "imagemetadata.h"

#include <set>
#include <unordered_set>
#include <unordered_map>

#include <QUrl>
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
#include <QPainter>

ImageDao *ImageDao::m_instance;

#define EXEC(sql) \
do { \
  if(!m_conn.exec(sql, SRC_LOCATION)) \
    goto error; \
} while(false)

ImageDao::ImageDao(QObject *parent) :
  QObject(parent),
  m_connPool(QStringLiteral("main.db"), SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE),
  m_conn(m_connPool.open())
{
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

  EXEC("PRAGMA journal_mode = WAL");

  if(!tableExists(QStringLiteral("store"))) {
    EXEC("CREATE TABLE store (id INTEGER PRIMARY KEY, hash TEXT UNIQUE, date INTEGER, image BLOB)");
    EXEC("CREATE TABLE tag (id INTEGER, tag TEXT, PRIMARY KEY (id, tag))");
  }

  if(!tableExists("meta")) {
    version = 0;
  } else {
    version = metaGet(QStringLiteral("version"));
  }

  qInfo("Database Version: %d", version.toInt());

  if(version < 1) {
    qInfo("Upgrading database format to 1");
    EXEC("CREATE TABLE meta (key TEXT PRIMARY KEY, type TEXT, value TEXT)");
    metaPut(QStringLiteral("version"), version = 1);
  }

  if(version < 2) {
    qInfo("Upgrading database format to 2");
    EXEC("ALTER TABLE store ADD COLUMN width INTEGER");
    EXEC("ALTER TABLE store ADD COLUMN height INTEGER");
    EXEC("ALTER TABLE store ADD COLUMN origin_url TEXT");
    metaPut(QStringLiteral("version"), version = 2);
  }

  if(version < 3) {
    qInfo("Upgrading database format to 3");
    EXEC("ALTER TABLE store ADD COLUMN phash INTEGER");
    metaPut(QStringLiteral("version"), version = 3);
  }

  if(version < 4) {
    qInfo("Upgrading database format to 4");

    EXEC("BEGIN");

    EXEC("CREATE TABLE new_store (id INTEGER PRIMARY KEY, hash TEXT UNIQUE, image BLOB)");
    EXEC("CREATE TABLE image (id INTEGER PRIMARY KEY, date INTEGER, width INTEGER, height INTEGER, phash INTEGER, origin_url TEXT)");
    EXEC("INSERT INTO new_store SELECT id, hash, image FROM store");
    EXEC("INSERT INTO image SELECT id, date, width, height, phash, origin_url FROM store");
    EXEC("DROP TABLE store");
    EXEC("ALTER TABLE new_store RENAME TO store");

    metaPut(QStringLiteral("version"), version = 4);

    EXEC("COMMIT");
  }

  if(version < 5) {
    qInfo("Upgrading database format to 5");
    EXEC("ALTER TABLE image ADD COLUMN deleted INTEGER");

    metaPut(QStringLiteral("version"), version = 5);
  }

  if(version < 11) {
    qInfo("Upgrading database format to 11");
    EXEC("CREATE TABLE IF NOT EXISTS thumb1280 (id INTEGER PRIMARY KEY, image BLOB)");
    EXEC("CREATE TABLE IF NOT EXISTS thumb640 (id INTEGER PRIMARY KEY, image BLOB)");
    EXEC("CREATE TABLE IF NOT EXISTS thumb320 (id INTEGER PRIMARY KEY, image BLOB)");
    EXEC("CREATE TABLE IF NOT EXISTS thumb160 (id INTEGER PRIMARY KEY, image BLOB)");
    EXEC("CREATE TABLE IF NOT EXISTS thumb80 (id INTEGER PRIMARY KEY, image BLOB)");
    EXEC("CREATE TABLE IF NOT EXISTS thumb40 (id INTEGER PRIMARY KEY, image BLOB)");
    metaPut(QStringLiteral("version"), version = 11);
  }

  if(version < 12) {
    qInfo("Upgrading database format to 12");
    EXEC("ALTER TABLE image ADD COLUMN format TEXT");
    metaPut(QStringLiteral("version"), version = 12);
  }

  if(version < 13) {
    qInfo("Upgrading database format to 13");
    EXEC("ALTER TABLE image ADD COLUMN filesize INTEGER");
    EXEC("ALTER TABLE image ADD COLUMN pixelformat INTEGER");
    metaPut(QStringLiteral("version"), version = 13);
  }
error:
  return;
}

#undef EXEC

ImageDao::~ImageDao()
{
  m_writeThread.quit();
  m_writeThread.wait();
  qInfo(SRC_LOCATION);
}

bool ImageDao::tableExists(const QString &table)
{
  auto ps = m_conn.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name=?1");
  ps.bind(1, table);
  return ps.step();
}

void ImageDao::metaPut(const QString &key, const QVariant &val)
{
  QMutexLocker writeLock(m_conn.writeLock());
  auto ps = m_conn.prepare("INSERT OR REPLACE INTO meta (key, type, value) VALUES (?1, ?2, ?3)");
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
  ps.step(SRC_LOCATION);
}

QVariant ImageDao::metaGet(const QString &key)
{
  QVariant result;

  auto ps = m_conn.prepare("SELECT type, value FROM meta WHERE key = ?1");
  ps.bind(1, key);
  if(ps.step(SRC_LOCATION)) {
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

QList<QObject *> ImageDao::findAllDuplicates(const QList<QObject *> &irefs, int maxDuplicates)
{
  return ::findAllDuplicates(irefs, maxDuplicates);
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

  auto ps = m_conn.prepare(
    "SELECT image.id, group_concat(tag, ' '), width, height, phash, deleted, format, filesize, pixelformat, origin_url "
    "FROM image LEFT JOIN tag ON (tag.id = image.id) "
    "WHERE image.deleted IS NULL OR image.deleted <= ?1"
    "GROUP BY image.id "
    "ORDER BY date ASC");

  ps.bind(1, (qint64)includeDeleted);

  QWriteLocker refMapLocker(&m_refMapLock);

  while(ps.step(SRC_LOCATION)) {
    ImageRef *ir = new ImageRef();
    ir->m_fileId = ps.resultInteger(0);
    ir->m_tags = QSet<QString>::fromList(ps.resultString(1).split(' ', QString::SkipEmptyParts));
    ir->m_size = { (int)ps.resultInteger(2), (int)ps.resultInteger(3) };
    ir->m_phash = ps.resultInteger(4);
    ir->m_deleted = ps.resultInteger(5);
    ir->m_format = ps.resultString(6);
    ir->m_fileSize = ps.resultInteger(7);
    ir->m_pixelFormat = (QImage::Format)ps.resultInteger(8);
    ir->m_url = ps.resultString(9);

    m_refMap.insert(ir->m_fileId, ir);
    result.append(ir);
  }

  qDebug() << SRC_LOCATION << timer.elapsed() << "ms";

  return result;
}

QStringList ImageDao::tagsById(qint64 id)
{
  auto ps = m_conn.prepare("SELECT tag FROM tag WHERE id = ?1");

  QStringList tags;

  ps.bind(1, id);
  while(ps.step()) {
    tags.append(ps.resultString(0));
  }
  ps.reset();

  return tags;
}

ImageRef *ImageDao::createImageRef(qint64 id)
{
  auto ps = m_conn.prepare("SELECT width, height, phash, deleted, format, filesize, pixelformat, origin_url FROM image WHERE id = ?1");
  ps.bind(1, id);
  if(!ps.step(SRC_LOCATION))
    return nullptr;

  ImageRef *iref = new ImageRef();
  iref->m_tags = QSet<QString>::fromList(tagsById(id));
  iref->m_fileId = id;
  iref->m_selected = false;
  iref->m_size = QSize(ps.resultInteger(0), ps.resultInteger(1));
  iref->m_phash = ps.resultInteger(2);
  iref->m_deleted = ps.resultInteger(3);
  iref->m_format = ps.resultString(4);
  iref->m_fileSize = ps.resultInteger(5);
  iref->m_pixelFormat = (QImage::Format)ps.resultInteger(6);
  iref->m_url = ps.resultString(7);

  QWriteLocker refMapLocker(&m_refMapLock);
  m_refMap.insert(iref->m_fileId, iref);
  return iref;
}

void ImageDao::compressImages(const QList<QObject *> &irefs)
{
  auto conn = m_connPool.open();
  QMutexLocker writeLock(connPool()->writeLock());

  for(QObject *ptr : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(ptr)) {
      RawImageQuery riq(conn, iref->m_fileId);
      QBuffer buffer(&riq.data);
      QImageReader reader(&buffer);
      QByteArray format = reader.format();
      if(format != "jpeg") {
        qDebug() << "Re-compressing" << iref->m_fileId;
        QImage image = reader.read();
        QBuffer outputBuffer;
        image.save(&outputBuffer, "jpeg", 95);
        QByteArray data = outputBuffer.data();
        qDebug() << "New size" << data.length();

        conn.exec("BEGIN");
        {
          auto ps = conn.prepare("UPDATE store SET image = ?1, hash = ?2 WHERE id = ?3");

          //SQLitePreparedStatement ps(idc.conn, );
          ps.bind(1, data);
          ps.bind(2, imageHash(data));
          ps.bind(3, iref->m_fileId);
          ps.exec(SRC_LOCATION);
        }

        {
          auto ps = conn.prepare("UPDATE image SET filesize = ?1, format = ?2 WHERE id = ?3");
          ps.bind(1, data.size());
          ps.bind(2, QStringLiteral("jpeg"));
          ps.bind(3, iref->m_fileId);
          ps.exec(SRC_LOCATION);
        }
        conn.exec("COMMIT");

        iref->m_format = QStringLiteral("jpeg");
        iref->m_fileSize = data.size();

        // force update
        emit iref->tagsChanged();
      }
    }
  }
}

void ImageDao::purgeDeletedImages()
{
  QMutexLocker writeLock(m_conn.writeLock());
  m_conn.exec("BEGIN", SRC_LOCATION);
  m_conn.exec("DELETE FROM store WHERE id IN (SELECT id FROM image WHERE deleted = 1)", SRC_LOCATION);
  m_conn.exec("DELETE FROM tag WHERE id IN (SELECT id FROM image WHERE deleted = 1)", SRC_LOCATION);
  m_conn.exec("DELETE FROM image WHERE deleted = 1", SRC_LOCATION);
  m_conn.exec("COMMIT", SRC_LOCATION);
  qInfo("Purged %d images from the database", sqlite3_changes(m_conn.m_db));
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
  auto conn = m_connPool.open();

  for(QObject *rptr : irefs) {
    ImageRef *ref = qobject_cast<ImageRef *>(rptr);

    RawImageQuery riq(conn, ref->m_fileId);
    QBuffer buffer(&riq.data);
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
        file.write(riq.data);
      }
    }

    qInfo() << SRC_LOCATION << "Rendering image:" << filename;
  }

  if(flags & FNAME_TO_CLIPBOARD) {
    QClipboard *cb = QGuiApplication::clipboard();
    cb->setText(clipBoardData.join('\n'));
  }
}

void ImageDao::fixImageMetaData(ImageProcessStatus *status)
{
  QThreadPool::globalInstance()->start(new FixImageMetaDataTask(status));
}

static bool greaterThan(const QSize &a, const QSize &b) {
  return a.width() > b.width() && a.height() > b.height();
}

static bool greaterThanOrEqual(const QSize &a, const QSize &b) {
  return a.width() >= b.width() && a.height() >= b.height();
}

static bool lessThan(const QSize &a, const QSize &b) {
  return !greaterThanOrEqual(a, b);
}

static QSize scaleOverlap(const QSize &actualSize, const QSize &requestedSize) {
  if(!requestedSize.isValid())
    return actualSize;

  float scalex = (float)requestedSize.width() / actualSize.width();
  float scaley = (float)requestedSize.height() / actualSize.height();

  if(scalex > scaley) {
    return { requestedSize.width(), int(actualSize.height() * scalex) / 2 * 2 };
  } else {
    return { int(actualSize.width() * scaley) / 2 * 2, requestedSize.height() };
  }
}

QImage ImageDao::makeThumbnail(SQLiteConnection *conn, ImageRef *iref, int thumbsize) {
  QSize thumbSize(thumbsize, thumbsize);

  {
    char sql[256];
    snprintf(sql, sizeof sql, "SELECT image FROM thumb%d WHERE id = ?1", thumbsize);
    auto ps = conn->prepare(sql);
    ps.bind(1, iref->m_fileId);
    ps.step(SRC_LOCATION);
    QByteArray thumbData = ps.resultBlobPointer(0);

    if(!thumbData.isNull()) {
      qDebug() << "Loading pre-existing thumbnail for" << iref->m_fileId << " Size" << thumbSize;
      QBuffer buffer(&thumbData);
      QImageReader imageReader(&buffer);
      return imageReader.read();
    }
  }

  QImage input;
  QImage result;

  if(thumbsize < 1280 && greaterThanOrEqual(iref->size(), thumbSize * 4)) {
    input = makeThumbnail(conn, iref, thumbsize * 2);
  }

  if(input.isNull()) {
    qDebug() << "Construct new thumbnail for" << iref->m_fileId << " Size" << thumbSize;
    // read from db
    RawImageQuery riq(*conn, iref->m_fileId);
    if(riq.data.isNull())
      return result;

    result = riq.decode(thumbSize);
    if(result.format() != QImage::Format_RGB32) {
      // convert transparent pixels to dark grey.
      QImage canvas(result.size(), QImage::Format_RGB32);
      canvas.fill(0xFF303030);
      {
        QPainter painter(&canvas);
        painter.drawImage(0, 0, result);
      }
      result = canvas;
    }
  } else {
    QSize newSize = scaleOverlap(input.size(), thumbSize);
    result = input.scaled(newSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }

  QBuffer outputBuffer;
  result.save(&outputBuffer, "JPEG", 92);

  {
    QMutexLocker lock(conn->writeLock());
    conn->exec("BEGIN", SRC_LOCATION);
    char sql[256];
    snprintf(sql, sizeof sql, "INSERT INTO thumb%d (id, image) VALUES (?1, ?2)", thumbsize);
    auto ps_w = conn->prepare(sql);

    ps_w.bind(1, iref->m_fileId);
    ps_w.bind(2, outputBuffer.buffer());
    ps_w.exec(SRC_LOCATION);
    conn->exec("COMMIT", SRC_LOCATION);
  }

  return result;
}

QImage ImageDao::requestImage(qint64 id, const QSize &requestedSize, volatile bool *cancelled)
{
  QImage result;

  if(*cancelled) {
    return result;
  }

  QSize actualSize;
  ImageRef *iref = nullptr;
  {
    QReadLocker refMapLocker(&m_refMapLock);
    iref = m_refMap.constFind(id).value();
    Q_ASSERT(iref != nullptr);
    actualSize = iref->m_size;
  }

  int thumbSizeList[] = { 40, 80, 160, 320, 640, 1280 };
  QSize thumbSize;
  for(int tsize : thumbSizeList) {
    QSize testSize(tsize, tsize);
    if(greaterThanOrEqual(testSize, requestedSize)) {
      thumbSize = testSize;
      break;
    }
  }

  if(thumbSize.isValid()) {
    QSize nextUpSize = thumbSize * 2;
    if(greaterThanOrEqual(actualSize, nextUpSize)) {
      auto conn = m_connPool.open();
      {
        char sql[256];
        snprintf(sql, sizeof sql, "SELECT image FROM thumb%d WHERE id = ?1", thumbSize.width());
        auto ps = conn.prepare(sql);
        ps.bind(1, id);
        ps.step(SRC_LOCATION);

        QByteArray thumbData = ps.resultBlobPointer(0);
        if(thumbData.isNull()) {
          ps.destroy();
          //qDebug() << "Thumb does not exist" << iref->m_fileId << thumbSize << requestedSize << actualSize;

          QImage thumbNail = makeThumbnail(&conn, iref, thumbSize.width());
          result = thumbNail.scaled(scaleOverlap(thumbNail.size(), requestedSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        } else if(!*cancelled) {
          //qDebug() << "Fastpath" << iref->m_fileId << thumbSize << requestedSize << actualSize;
          QBuffer buffer(&thumbData);
          QImageReader imageReader(&buffer);
          imageReader.setScaledSize(scaleOverlap(imageReader.size(), requestedSize));
          result = imageReader.read();
        }
      }
    }
  }

  if(!result.isNull() || *cancelled) {
    return result;
  }

  auto conn = m_connPool.open();

  RawImageQuery riq(conn, id);
  if(!(*cancelled)) {
    result = riq.decode(requestedSize);
  }

  return result;
}

QString ImageDao::imageHash(const QByteArray &data)
{
  QByteArray hashBytes = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
  return hashBytes.toHex();
}

ImageDao *ImageDao::instance()
{
  if(!m_instance) {
    m_instance = new ImageDao();
  }

  return m_instance;
}

void ImageDaoDeferredWriter::startWrite() {
  if(!m_inTransaction) {
    m_conn.writeLock()->lock();
    m_conn.exec("BEGIN", SRC_LOCATION);
    m_inTransaction = true;
    QTimer::singleShot(0, this, &ImageDaoDeferredWriter::endWrite);
  }
}

ImageDaoDeferredWriter::ImageDaoDeferredWriter(SQLiteConnection &&conn, QObject *parent) : m_conn(std::move(conn)), QObject(parent)
{

}

ImageDaoDeferredWriter::~ImageDaoDeferredWriter()
{
}

void ImageDaoDeferredWriter::endWrite() {
  if(m_inTransaction) {
    m_conn.exec("COMMIT", SRC_LOCATION);
    m_inTransaction = false;
    m_conn.writeLock()->unlock();
  }
}

void ImageDaoDeferredWriter::addTag(const QList<QObject *> &irefs, const QString &tag)
{
  startWrite();
  auto ps = m_conn.prepare("INSERT OR IGNORE INTO tag (id, tag) VALUES (?1, ?2)");
  for(QObject *qobj : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(qobj)) {
      ps.bind(1, iref->m_fileId);
      ps.bind(2, tag);
      ps.exec(SRC_LOCATION);
    }
  }
}

void ImageDaoDeferredWriter::removeTag(const QList<QObject *> &irefs, const QString &tag)
{
  startWrite();
  auto ps = m_conn.prepare("DELETE FROM tag WHERE id = ?1 AND tag = ?2");
  for(QObject *qobj : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(qobj)) {
      ps.bind(1, iref->m_fileId);
      ps.bind(2, tag);
      ps.exec(SRC_LOCATION);
    }
  }
}

void ImageDaoDeferredWriter::updateDeleted(const QList<QObject *> &irefs, bool deleted)
{
  startWrite();
  auto ps = m_conn.prepare("UPDATE image SET deleted = ?1 WHERE id = ?2");
  for(QObject *ptr : irefs) {
    if(auto iref = qobject_cast<ImageRef *>(ptr)) {
      ps.bind(1, (qint64)deleted);
      ps.bind(2, iref->m_fileId);
      ps.exec(SRC_LOCATION);
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
  QString hash = ImageDao::imageHash(data);

  {
    auto ps = m_conn.prepare("INSERT OR IGNORE INTO store (hash, image) VALUES (?1, ?2)");
    ps.bind(1, hash);
    ps.bind(2, data);
    ps.exec(SRC_LOCATION);
  }

  if(sqlite3_changes(m_conn.m_db) == 0) {
    qWarning("Duplicate image not inserted");
    return;
  }

  qint64 last_id = sqlite3_last_insert_rowid(m_conn.m_db);
  qInfo() << "Inserted image with ID" << last_id << "from" << url.toString();

  {
    auto ps = m_conn.prepare("INSERT INTO image (id, date, origin_url) VALUES (?1, datetime(), ?2)");
    ps.bind(1, last_id);
    ps.bind(2, url.toString());
    ps.exec(SRC_LOCATION);
  }

  updateImageMetaData(&m_conn, data, last_id);

  endWrite();

  emit writeComplete(url, last_id);
}

QImage RawImageQuery::decode(const QSize &size) {
  QBuffer buffer(&data);
  QImageReader reader(&buffer);
  if(size.isValid()) {
    reader.setScaledSize(scaleOverlap(reader.size(), size));
  }
  return reader.read();
}
