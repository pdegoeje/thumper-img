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
  if(sqlite3_open_v2("test.db", &m_db, SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    qWarning("Coudn't open SQLite database: %s", sqlite3_errmsg(m_db));
  }

  CHECK_EXEC("CREATE TABLE IF NOT EXISTS store (id TEXT PRIMARY KEY, date INTEGER, image BLOB)");
  CHECK_EXEC("CREATE TABLE IF NOT EXISTS tag (id TEXT, tag TEXT, PRIMARY KEY (id, tag))");
  CHECK_EXEC("CREATE INDEX IF NOT EXISTS tag_index ON tag ( tag )")
error:
  return;
}

ImageDao::~ImageDao()
{
  sqlite3_close(m_db);
}

void ImageDao::addTag(const QString &id, const QString &tag)
{
  QStringList tags;

  sqlite3_stmt *stmt = nullptr;

  CHECK(sqlite3_prepare_v2(m_db, "INSERT OR IGNORE INTO tag (id, tag) VALUES ( ?1, ?2 )", -1, &stmt, nullptr));
  CHECK(sqlite3_bind_text(stmt, 1, qUtf8Printable(id), -1, SQLITE_TRANSIENT));
  CHECK(sqlite3_bind_text(stmt, 2, qUtf8Printable(tag), -1, SQLITE_TRANSIENT));
  CHECK_STEP(stmt);

error:
  sqlite3_finalize(stmt);
  return;
}

void ImageDao::removeTag(const QString &id, const QString &tag)
{
  QStringList tags;

  sqlite3_stmt *stmt = nullptr;

  CHECK(sqlite3_prepare_v2(m_db, "DELETE FROM tag WHERE id = ?1 AND tag = ?2", -1, &stmt, nullptr));
  CHECK(sqlite3_bind_text(stmt, 1, qUtf8Printable(id), -1, SQLITE_TRANSIENT));
  CHECK(sqlite3_bind_text(stmt, 2, qUtf8Printable(tag), -1, SQLITE_TRANSIENT));
  CHECK_STEP(stmt);

error:
  sqlite3_finalize(stmt);
  return;
}

QStringList ImageDao::tagsById(const QString &id)
{
  QStringList tags;

  sqlite3_stmt *stmt = nullptr;

  CHECK(sqlite3_prepare_v2(m_db, "SELECT tag FROM tag WHERE id = ?1", -1, &stmt, nullptr));
  CHECK(sqlite3_bind_text(stmt, 1, qUtf8Printable(id), -1, SQLITE_TRANSIENT));

  STEP_LOOP_BEGIN(stmt) {
      const char *data = (const char *)sqlite3_column_text(stmt, 0);
      tags.append(QString::fromUtf8(data));
  } STEP_LOOP_END;

error:
  sqlite3_finalize(stmt);
  return tags;
}

QStringList ImageDao::allTags()
{
  QStringList tags;

  sqlite3_stmt *stmt = nullptr;

  CHECK(sqlite3_prepare_v2(m_db, "SELECT DISTINCT tag FROM tag", -1, &stmt, nullptr));
  STEP_LOOP_BEGIN(stmt) {
    const char *data = (const char *)sqlite3_column_text(stmt, 0);
    tags.append(QString::fromUtf8(data));
  } STEP_LOOP_END;

error:
  sqlite3_finalize(stmt);
  return tags;
}

QImage ImageDao::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
  sqlite3_stmt *stmt = nullptr;
  QImage result;

  CHECK(sqlite3_prepare_v2(m_db, "SELECT image FROM store WHERE id = ?1", -1, &stmt, nullptr));
  CHECK(sqlite3_bind_text(stmt, 1, qUtf8Printable(id), -1, SQLITE_TRANSIENT));

  STEP_SINGLE(stmt);

  {
    const char *data = (const char *)sqlite3_column_blob(stmt, 0);
    int bytes = sqlite3_column_bytes(stmt, 0);

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
  }

error:
  sqlite3_finalize(stmt);
  return result;
}

void ImageDao::insert(const QString &id, const QByteArray &data)
{
  sqlite3_stmt *stmt = nullptr;

  CHECK(sqlite3_prepare_v2(m_db, "INSERT OR IGNORE INTO store (id, date, image) VALUES (?1, datetime(), ?2)", -1, &stmt, nullptr));
  CHECK(sqlite3_bind_text(stmt, 1, qUtf8Printable(id), -1, SQLITE_TRANSIENT));
  CHECK(sqlite3_bind_blob(stmt, 2, data.data(), data.length(), SQLITE_TRANSIENT));
  CHECK_STEP(stmt);

error:
  sqlite3_finalize(stmt);
}

QStringList ImageDao::allIds()
{
  QStringList ids;

  sqlite3_stmt *stmt = nullptr;

  CHECK(sqlite3_prepare_v2(m_db, "SELECT id FROM store ORDER BY date", -1, &stmt, nullptr));
  STEP_LOOP_BEGIN(stmt) {
    const char *data = (const char *)sqlite3_column_text(stmt, 0);
    ids.append(QString::fromUtf8(data));
  } STEP_LOOP_END;

error:
  sqlite3_finalize(stmt);
  return ids;
}

QStringList ImageDao::idsByTags(const QStringList &tags)
{
  QStringList result;

  createTemporaryTable(QStringLiteral("taglist"), tags);

  sqlite3_stmt *stmt = nullptr;
  CHECK(sqlite3_prepare_v2(m_db, "SELECT tag.id, count(*) as count FROM tag LEFT JOIN store ON (tag.id = store.id) WHERE tag IN (SELECT item FROM taglist) GROUP BY tag.id HAVING count = ?1 ORDER BY date", -1, &stmt, nullptr));
  CHECK(sqlite3_bind_int(stmt, 1, tags.length()));

  STEP_LOOP_BEGIN(stmt) {
    const char *id = (const char *)sqlite3_column_text(stmt, 0);
    result.push_back(QString::fromUtf8(id));
  } STEP_LOOP_END;

error:
  sqlite3_finalize(stmt);
  return result;
}

QVariantList ImageDao::tagsByMultipleIds(const QStringList &ids)
{
  QVariantList result;
  sqlite3_stmt *stmt = nullptr;

  createSelectionTable(ids);

  CHECK(sqlite3_prepare_v2(m_db, "SELECT tag, count(*) as count FROM tag WHERE id IN (SELECT item FROM selection) GROUP BY tag ORDER BY count DESC", -1, &stmt, nullptr));

  STEP_LOOP_BEGIN(stmt) {
    const char *data = (const char *)sqlite3_column_text(stmt, 0);
    int count = sqlite3_column_int(stmt, 1);
    QVariantList kv = { QVariant::fromValue(QString::fromUtf8(data)), QVariant::fromValue(count) };
    result.push_back(kv);
  } STEP_LOOP_END;

error:
  sqlite3_finalize(stmt);
  return result;
}

void ImageDao::addTagToMultipleIds(const QStringList &ids, const QString &tag)
{
  createSelectionTable(ids);

  sqlite3_stmt *stmt = nullptr;
  if(sqlite3_prepare_v2(m_db, "INSERT OR IGNORE INTO tag (id, tag) SELECT item, ?1 FROM selection", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 1, qUtf8Printable(tag), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_step(stmt) != SQLITE_DONE)
    goto error;

  goto done;
error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
done:
  sqlite3_finalize(stmt);
  destroySelectionTable();
}

void ImageDao::removeTagFromMultipleIds(const QStringList &ids, const QString &tag)
{
  createSelectionTable(ids);

  sqlite3_stmt *stmt = nullptr;
  if(sqlite3_prepare_v2(m_db, "DELETE FROM tag WHERE tag = ?1 AND id IN (SELECT item FROM selection)", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 1, qUtf8Printable(tag), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_step(stmt) != SQLITE_DONE)
    goto error;

  qInfo("removeTagFromMultipleIds");

  goto done;
error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
done:
  sqlite3_finalize(stmt);
  destroySelectionTable();
}

QVariantList ImageDao::allTagsCount()
{
  QVariantList result;

  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "SELECT tag, count(*) as count FROM tag GROUP BY tag ORDER BY count DESC", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  while(sqlite3_step(stmt) == SQLITE_ROW) {
    const char *data = (const char *)sqlite3_column_text(stmt, 0);
    int count = sqlite3_column_int(stmt, 1);
    QVariantList kv = { QVariant::fromValue(QString::fromUtf8(data)), QVariant::fromValue(count) };
    result.push_back(kv);
  }

  goto done;
error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
done:
  sqlite3_finalize(stmt);
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

void ImageDao::destroyTemporaryTable(const QString &tableName)
{

}

void ImageDao::createSelectionTable(const QStringList &ids)
{
  createTemporaryTable(QStringLiteral("selection"), ids);
}

void ImageDao::destroySelectionTable()
{

}
