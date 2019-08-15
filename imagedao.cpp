#include "imagedao.h"
#include "sqlite3.h"

#include <QImage>
#include <QImageReader>
#include <QBuffer>

ImageDao *ImageDao::m_instance;

ImageDao::ImageDao(QObject *parent) : QObject(parent)
{
  qInfo("construct ImageDao");

  if(sqlite3_open_v2("test.db", &m_db, SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    qWarning("Coudn't open SQLite database: %s", sqlite3_errmsg(m_db));
  }

  char *error = nullptr;

  if(sqlite3_exec(m_db, "CREATE TABLE IF NOT EXISTS store (id TEXT PRIMARY KEY, image BLOB)", nullptr, nullptr, &error) != SQLITE_OK)
    goto error;

  if(sqlite3_exec(m_db, "CREATE TABLE IF NOT EXISTS tag (id TEXT, tag TEXT, PRIMARY KEY (id, tag))", nullptr, nullptr, &error) != SQLITE_OK)
    goto error;

  if(sqlite3_exec(m_db, "CREATE INDEX IF NOT EXISTS tag_index ON tag ( tag )", nullptr, nullptr, &error) != SQLITE_OK)
    goto error;

  return;

error:
  qWarning("SQLite error: %s", error);
  sqlite3_free(error);
}

ImageDao::~ImageDao()
{
  sqlite3_close(m_db);
}

void ImageDao::addTag(const QString &id, const QString &tag)
{
  QStringList tags;

  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "INSERT OR IGNORE INTO tag (id, tag) VALUES ( ?1, ?2 )", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 1, qUtf8Printable(id.toUtf8()), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 2, qUtf8Printable(tag.toUtf8()), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_step(stmt) != SQLITE_DONE)
    goto error;

  sqlite3_finalize(stmt);
  return;

error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
  sqlite3_finalize(stmt);
  return;
}

void ImageDao::removeTag(const QString &id, const QString &tag)
{
  QStringList tags;

  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "DELETE FROM tag WHERE id = ?1 AND tag = ?2", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 1, qUtf8Printable(id.toUtf8()), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 2, qUtf8Printable(tag.toUtf8()), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_step(stmt) != SQLITE_DONE)
    goto error;

  sqlite3_finalize(stmt);
  return;

error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
  sqlite3_finalize(stmt);
  return;
}

QStringList ImageDao::tagsById(const QString &id)
{
  QStringList tags;

  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "SELECT tag FROM tag WHERE id = ?1", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 1, qUtf8Printable(id.toUtf8()), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  while(sqlite3_step(stmt) == SQLITE_ROW) {
    const char *data = (const char *)sqlite3_column_text(stmt, 0);
    tags.append(QString::fromUtf8(data));
  }

  sqlite3_finalize(stmt);
  return tags;

error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
  sqlite3_finalize(stmt);
  return tags;
}

QStringList ImageDao::allTags()
{
  QStringList tags;

  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "SELECT DISTINCT tag FROM tag", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  while(sqlite3_step(stmt) == SQLITE_ROW) {
    const char *data = (const char *)sqlite3_column_text(stmt, 0);
    tags.append(QString::fromUtf8(data));
  }

  sqlite3_finalize(stmt);
  return tags;
error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
  sqlite3_finalize(stmt);
  return tags;
}

QImage ImageDao::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "SELECT image FROM store WHERE id = ?1", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 1, qUtf8Printable(id.toUtf8()), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_step(stmt) != SQLITE_ROW)
    goto error;

  const char *data = (const char *)sqlite3_column_blob(stmt, 0);
  int bytes = sqlite3_column_bytes(stmt, 0);

  {
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

    reader.setScaledSize(newSize);

//    qInfo("Loading image size: %dx%d scaled %dx%d", requestedSize.width(), requestedSize.height(), newSize.width(), newSize.height());

    QImage result = reader.read();

    if(size) {
      *size = QSize(result.width(), result.height());
    }

    sqlite3_finalize(stmt);
    return result;
  }

error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
  sqlite3_finalize(stmt);
  return {};
}

void ImageDao::insert(const QString &id, const QByteArray &data)
{
  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "INSERT OR IGNORE INTO store (id, image) VALUES (?1, ?2)", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 1, qUtf8Printable(id.toUtf8()), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_blob(stmt, 2, data.data(), data.length(), SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_step(stmt) != SQLITE_DONE)
    goto error;

  sqlite3_finalize(stmt);
  return;

error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
  sqlite3_finalize(stmt);
}

QStringList ImageDao::loadExistingIds()
{
  QStringList ids;

  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "SELECT id FROM store", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  while(sqlite3_step(stmt) == SQLITE_ROW) {
    const char *data = (const char *)sqlite3_column_text(stmt, 0);
    ids.append(QString::fromUtf8(data));
  }

  sqlite3_finalize(stmt);
  return ids;

error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
  sqlite3_finalize(stmt);
  return ids;
}

ImageDao *ImageDao::instance()
{
  if(!m_instance) {
    m_instance = new ImageDao();
  }

  return m_instance;
}
