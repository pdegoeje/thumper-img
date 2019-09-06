#include "imagedao.h"
#include "sqlite3.h"

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

bool ImageDao::addTag(ImageRef *iref, const QString &tag) {
  if(iref->m_tags.contains(tag))
    return false;

  iref->m_tags.insert(tag);
  addTag(iref->m_fileId, tag);
  emit iref->tagsChanged();

  return true;
}

bool ImageDao::removeTag(ImageRef *iref, const QString &tag) {
  if(!iref->m_tags.contains(tag))
    return false;

  iref->m_tags.remove(tag);
  removeTag(iref->m_fileId, tag);
  emit iref->tagsChanged();

  return true;
}

QList<QObject *> ImageDao::addTagMultiple(const QList<QObject *> &irefs, const QString &tag)
{
  QList<QObject *> result;
  transactionStart();

  for(QObject *obj : irefs) {
    auto iref = qobject_cast<ImageRef *>(obj);
    if(addTag(iref, tag)) {
      result.append(iref);
    }
  }

  transactionEnd();
  return result;
}

QList<QObject *> ImageDao::removeTagMultiple(const QList<QObject *> &irefs, const QString &tag)
{
  QList<QObject *> result;
  transactionStart();

  for(QObject *obj : irefs) {
    auto iref = qobject_cast<ImageRef *>(obj);
    if(removeTag(iref, tag)) {
      result.append(iref);
    }
  }

  transactionEnd();
  return result;
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

QList<QObject *> ImageDao::all()
{
  QElapsedTimer timer;
  timer.start();

  QList<QObject *> result;

  SQLitePreparedStatement m_ps_all(m_conn,
    "SELECT store.id, group_concat(tag, ' '), width, height, phash "
    "FROM store LEFT JOIN tag ON (tag.id = store.id) "
    "GROUP BY store.id "
    "ORDER BY date ASC");

  while(m_ps_all.step(__FUNCTION__)) {
    ImageRef *ir = new ImageRef();
    ir->m_fileId = m_ps_all.resultInteger(0);
    ir->m_tags = QSet<QString>::fromList(m_ps_all.resultString(1).split(' ', QString::SkipEmptyParts));
    ir->m_size = { (int)m_ps_all.resultInteger(2), (int)m_ps_all.resultInteger(3) };
    ir->m_phash = m_ps_all.resultInteger(4);
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

  void run() override {
    ImageDao *dao = ImageDao::instance();
    SQLiteConnection *conn = dao->connPool()->open();

    conn->exec("BEGIN TRANSACTION", __FUNCTION__);
    {
      SQLitePreparedStatement ps_update(conn, "UPDATE store SET width = ?1, height = ?2, phash = ?3 WHERE id = ?4");
      SQLitePreparedStatement ps(conn, "SELECT id FROM store WHERE width IS NULL OR phash IS NULL");
      qreal progress = 0.0;
      while(ps.step(__FUNCTION__)) {
        qint64 id = ps.resultInteger(0);

        ImageDao::ImageDataContext idc;
        dao->imageDataAcquire(idc, id);
        QBuffer buffer(&idc.data);
        QImageReader reader(&buffer);

        QSize size = reader.size();

        reader.setBackgroundColor(Qt::darkGray);
        reader.setScaledSize({8, 8});

        QImage image = reader.read();
        if(!image.isNull()) {
          image.convertTo(QImage::Format_Grayscale8);
          Q_ASSERT(image.width() == 8);
          Q_ASSERT(image.height() == 8);

          int acc = 0;

          for(int y = 0; y < 8; y++) {
            const uchar *scanLine = image.constScanLine(y);
            for(int x = 0; x < 8; x++) {
              acc += scanLine[x];
            }
          }

          int average = (acc + 32) / 64;

          quint64 phash = 0;

          for(int y = 0; y < 8; y++) {
            const uchar *scanLine = image.constScanLine(y);
            for(int x = 0; x < 8; x++) {
              phash <<= 1;
              phash |= !!(scanLine[x] > average);
            }
          }

          qDebug() << id << "phash" << phash << "average" << average;

          qDebug() << __FUNCTION__ << size;
          ps_update.bind(1, size.width());
          ps_update.bind(2, size.height());
          ps_update.bind(3, phash);
          ps_update.bind(4, id);
          ps_update.exec(__FUNCTION__);
        }

        //QThread::sleep(1);
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

void SQLitePreparedStatement::init(sqlite3 *db, const char *statement)
{
  if(sqlite3_prepare_v2(db, statement, -1, &m_stmt, nullptr) != SQLITE_OK) {
    qWarning("Failed to prepare statement: %s", sqlite3_errmsg(db));
  }
}

void SQLitePreparedStatement::exec(const char *debug_str)
{
  step(debug_str);
  reset();
}

void SQLitePreparedStatement::bind(int param, const QString &text)
{
  if(sqlite3_bind_text16(m_stmt, param, text.constData(), text.size() * 2, SQLITE_TRANSIENT) != SQLITE_OK) {
    qWarning("Failed to bind: %s", sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
  }
}

void SQLitePreparedStatement::bind(int param, qint64 value)
{
  if(sqlite3_bind_int64(m_stmt, param, value) != SQLITE_OK) {
    qWarning("Failed to bind: %s", sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
  }
}

void SQLitePreparedStatement::bind(int param, const QByteArray &data)
{
  if(sqlite3_bind_blob(m_stmt, param, data.data(), data.length(), SQLITE_TRANSIENT) != SQLITE_OK) {
    qWarning("Failed to bind: %s", sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
  }
}

QString SQLitePreparedStatement::resultString(int index)
{
  return QString((const QChar *)sqlite3_column_text16(m_stmt, index));
}

qint64 SQLitePreparedStatement::resultInteger(int index)
{
  return sqlite3_column_int64(m_stmt, index);
}

bool SQLitePreparedStatement::step(const char *debug_str)
{
  int rval = sqlite3_step(m_stmt);
  if(rval == SQLITE_ROW) {
    return true;
  }

  if(rval == SQLITE_DONE) {
    return false;
  }

  qWarning("Failed to step (%s): %d %s", debug_str, rval, sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
  return false;
}

void SQLitePreparedStatement::reset()
{
  sqlite3_reset(m_stmt);
}

void SQLitePreparedStatement::clear()
{
  sqlite3_clear_bindings(m_stmt);
}

void SQLitePreparedStatement::destroy()
{
  sqlite3_finalize(m_stmt);
  m_stmt = nullptr;
}

SQLitePreparedStatement::SQLitePreparedStatement(SQLiteConnection *conn, const char *sql)
{
  init(conn->m_db, sql);
}

QStringList ImageRef::tags()
{
  return m_tags.toList();
}

SQLiteConnection::SQLiteConnection(const QString &dbname, int flags)
{
  if(sqlite3_open_v2(qUtf8Printable(dbname), &m_db, flags, nullptr) != SQLITE_OK) {
    qWarning("Couldn't open SQLite database: %s", sqlite3_errmsg(m_db));
  }

  qInfo("Created new connection to %s", qUtf8Printable(dbname));
}

SQLiteConnection::~SQLiteConnection()
{
  sqlite3_close_v2(m_db);
}

bool SQLiteConnection::exec(const char *sql, const char *debug_str)
{
  char *errmsg;
  if(sqlite3_exec(m_db, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
    qWarning("Failed to exec (%s): %s", debug_str, errmsg);
    return false;
  }

  return true;
}
