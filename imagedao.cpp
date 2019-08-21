#include "imagedao.h"
#include "sqlite3.h"

#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QThread>
#include <QElapsedTimer>
#include <QDebug>

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

  CHECK_EXEC("PRAGMA journal_mode = WAL");
  CHECK_EXEC("CREATE TABLE IF NOT EXISTS store (id INTEGER PRIMARY KEY, hash TEXT UNIQUE, date INTEGER, image BLOB)");
  CHECK_EXEC("CREATE TABLE IF NOT EXISTS tag (id INTEGER, tag TEXT, PRIMARY KEY (id, tag))");
  CHECK_EXEC("CREATE INDEX IF NOT EXISTS tag_index ON tag ( tag )");

  createTemporaryTable("search", {});

  m_ps_addTag.init(m_db, "INSERT OR IGNORE INTO tag (id, tag) VALUES (?1, ?2)");
  m_ps_allTagCount.init(m_db, "SELECT tag, count(id) FROM tag GROUP BY tag ORDER BY tag");

  m_ps_removeTag.init(m_db, "DELETE FROM tag WHERE id = ?1 AND tag = ?2");
  m_ps_tagsById.init(m_db, "SELECT tag FROM tag WHERE id = ?1");
  m_ps_idByHash.init(m_db, "SELECT id FROM store WHERE hash = ?1");

  m_ps_all.init(m_db, "SELECT store.id, group_concat(tag, ' ') "
                      "FROM store LEFT JOIN tag ON (tag.id = store.id) "
                      "GROUP BY store.id "
                      "ORDER BY date DESC");


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

QVariantList ImageDao::allTagCount()
{
  QVariantList result;
  while(m_ps_allTagCount.step()) {
    QVariantList r = { m_ps_allTagCount.resultString(0), m_ps_allTagCount.resultInteger(1) };
    result.append(QVariant::fromValue(r));
  }
  m_ps_allTagCount.reset();
  return result;
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

  while(m_ps_all.step(__FUNCTION__)) {
    ImageRef *ir = new ImageRef();
    ir->m_fileId = m_ps_all.resultInteger(0);
    ir->m_tags = QSet<QString>::fromList(m_ps_all.resultString(1).split(' ', QString::SkipEmptyParts));
    result.append(ir);
  }

  m_ps_all.reset();

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

void ImageDao::transactionStart()
{
  m_ps_transStart.exec();
}

void ImageDao::transactionEnd()
{
  m_ps_transEnd.exec();
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

QImage ImageDao::requestImage(qint64 id, const QSize &requestedSize, volatile bool *cancelled)
{
  QImage result;

  SQLitePreparedStatement m_ps_imageById;
  SQLiteConnection *m_myConn = m_connPool.open();

  m_ps_imageById.init(m_myConn->m_db, "SELECT image FROM store WHERE id = ?1");
  m_ps_imageById.bind(1, id);
  m_ps_imageById.step(__FUNCTION__);

  const char *data = (const char *)sqlite3_column_blob(m_ps_imageById.m_stmt, 0);
  int bytes = sqlite3_column_bytes(m_ps_imageById.m_stmt, 0);

  auto byte_array = QByteArray::fromRawData(data, bytes);

  if(!(*cancelled)) {
    QBuffer buffer(&byte_array);
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

  m_ps_imageById.reset();
  m_ps_imageById.destroy();

  m_connPool.close(m_myConn);

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

void SQLitePreparedStatement::exec()
{
  step();
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

QStringList ImageRef::tags()
{
  return m_tags.toList();
}

SQLiteConnection::SQLiteConnection(const QString &dbname, int flags)
{
  if(sqlite3_open_v2(qUtf8Printable(dbname), &m_db, flags, nullptr) != SQLITE_OK) {
    qWarning("Coudln't open SQLite database: %s", sqlite3_errmsg(m_db));
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
    qWarning("Failed to exec (%s): %s", errmsg);
    return false;
  }

  return true;
}
