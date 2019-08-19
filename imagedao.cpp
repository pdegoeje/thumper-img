#include "imagedao.h"
#include "sqlite3.h"

#include <QImage>
#include <QImageReader>
#include <QBuffer>

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

ImageDao::ImageDao(QObject *parent) : QObject(parent)
{
  if(sqlite3_open_v2("main.db", &m_db, SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    qWarning("Coudn't open SQLite database: %s", sqlite3_errmsg(m_db));
  }

  CHECK_EXEC("CREATE TABLE IF NOT EXISTS store (id INTEGER PRIMARY KEY, hash TEXT UNIQUE, date INTEGER, image BLOB)");
  CHECK_EXEC("CREATE TABLE IF NOT EXISTS tag (id INTEGER, tag TEXT, PRIMARY KEY (id, tag))");
  CHECK_EXEC("CREATE INDEX IF NOT EXISTS tag_index ON tag ( tag )");

  createTemporaryTable("search", {});

  m_ps_insert.init(m_db, "INSERT OR IGNORE INTO store (hash, date, image) VALUES (?1, datetime(), ?2)");
  m_ps_addTag.init(m_db, "INSERT OR IGNORE INTO tag (id, tag) VALUES (?1, ?2)");
  //m_ps_addTagToList.init(m_db, "INSERT OR IGNORE INTO tag (id, tag) SELECT item, ?1 FROM selection");

  m_ps_removeTag.init(m_db, "DELETE FROM tag WHERE id = ?1 AND tag = ?2");
  m_ps_tagsById.init(m_db, "SELECT tag FROM tag WHERE id = ?1");
  m_ps_idByHash.init(m_db, "SELECT id FROM store WHERE hash = ?1");
  m_ps_search.init(m_db, "SELECT tag.id, group_concat(tag, ' ')"
                           "FROM tag JOIN store ON (tag.id = store.id) "
                           "WHERE tag IN (SELECT * FROM search) "
                           "GROUP BY tag.id "
                           "HAVING count(tag.id) = (SELECT count(*) FROM search) "
                           "ORDER BY date DESC");

  m_ps_all.init(m_db, "SELECT store.id, group_concat(tag, ' ') "
                      "FROM store LEFT JOIN tag ON (tag.id = store.id) "
                      "GROUP BY store.id "
                      "ORDER BY date DESC");

  m_ps_imageById.init(m_db, "SELECT image FROM store WHERE id = ?1");
  m_ps_transStart.init(m_db, "BEGIN TRANSACTION");
  m_ps_transEnd.init(m_db, "END TRANSACTION");
error:
  return;
}

ImageDao::~ImageDao()
{
  //m_ps_removeTag.destroy();
  //m_ps_addTag.destroy();

  sqlite3_close(m_db);
}

void ImageDao::addTagToSelection(QList<QObject *> selection, const QString &tag) {
  transactionStart();
  for(QObject *irobj : selection) {
    ImageRef *ir = qobject_cast<ImageRef *>(irobj);
    if(ir != nullptr) {
      addTag(ir, tag);
    }
  }
  transactionEnd();
}

QList<QObject *> ImageDao::search(const QStringList &tags)
{
  createTemporaryTable("search", tags);

  QList<QObject *> result;

  while(m_ps_search.step()) {
    ImageRef *ir = new ImageRef();
    ir->m_fileId = m_ps_search.resultInteger(0);
    ir->m_tags = QSet<QString>::fromList(m_ps_search.resultString(1).split(' ', QString::SkipEmptyParts));
    result.append(ir);
  }

  m_ps_search.reset();

  return result;
}

QList<QObject *> ImageDao::all()
{
  QList<QObject *> result;

  while(m_ps_all.step()) {
    ImageRef *ir = new ImageRef();
    ir->m_fileId = m_ps_all.resultInteger(0);
    ir->m_tags = QSet<QString>::fromList(m_ps_all.resultString(1).split(' ', QString::SkipEmptyParts));
    result.append(ir);
  }

  m_ps_all.reset();

  return result;
}

void ImageDao::addTag(qint64 id, const QString &tag)
{
  m_ps_addTag.bind(1, id);
  m_ps_addTag.bind(2, tag);
  m_ps_addTag.step();
  m_ps_addTag.reset();
}

void ImageDao::removeTag(qint64 id, const QString &tag)
{
  m_ps_removeTag.bind(1, id);
  m_ps_removeTag.bind(2, tag);
  m_ps_removeTag.step();
  m_ps_removeTag.reset();
}

ImageRef *ImageDao::findHash(const QString &hash)
{
  m_ps_idByHash.bind(1, hash);
  if(!m_ps_idByHash.step())
    return nullptr;

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

QImage ImageDao::requestImage(qint64 id, QSize *size, const QSize &requestedSize)
{
  QImage result;

  m_ps_imageById.bind(1, id);
  m_ps_imageById.step();

  const char *data = (const char *)sqlite3_column_blob(m_ps_imageById.m_stmt, 0);
  int bytes = sqlite3_column_bytes(m_ps_imageById.m_stmt, 0);

  auto byte_array = QByteArray::fromRawData(data, bytes);

  QBuffer buffer(&byte_array);
  QImageReader reader(&buffer);

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

  if(newSize.height() < 1)
    newSize.setHeight(1);

  if(newSize.width() < 1)
    newSize.setWidth(1);

  reader.setScaledSize(newSize);

  result = reader.read();
  if(size) {
    *size = QSize(result.width(), result.height());
  }

  m_ps_imageById.reset();
  return result;
}

void ImageDao::insert(const QString &hash, const QByteArray &data)
{
  m_ps_insert.bind(1, hash);
  m_ps_insert.bind(2, data);
  m_ps_insert.step();
  m_ps_insert.reset();
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

void ImageDao::destroyTemporaryTable(const QString &tableName)
{

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

bool SQLitePreparedStatement::step()
{
  int rval = sqlite3_step(m_stmt);
  if(rval == SQLITE_ROW) {
    return true;
  }

  if(rval == SQLITE_DONE) {
    return false;
  }

  qWarning("Failed to step: %s", sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
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
