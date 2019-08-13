#include "thumperimageprovider.h"

#include "sqlite3.h"

#include <QBuffer>
#include <QImageReader>

ThumperImageProvider *ThumperImageProvider::m_instance;

ThumperImageProvider::ThumperImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {
  if(sqlite3_open_v2("test.db", &m_db, SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    qWarning("Coudn't open SQLite database: %s", sqlite3_errmsg(m_db));
  }

  char *error = nullptr;
  if(sqlite3_exec(m_db, "CREATE TABLE IF NOT EXISTS store (id TEXT PRIMARY KEY, image BLOB)", nullptr, nullptr, &error) != SQLITE_OK) {
    qWarning("SQLite error: %s", error);
    sqlite3_free(error);
  }
}

ThumperImageProvider::~ThumperImageProvider()
{
  sqlite3_close(m_db);
}

QImage ThumperImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
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

void ThumperImageProvider::insert(const QString &id, const QByteArray &data)
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

QVector<QString> ThumperImageProvider::loadExistingIds()
{
  QVector<QString> ids;

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

ThumperImageProvider *ThumperImageProvider::instance() {
  if(m_instance == nullptr) {
    m_instance = new ThumperImageProvider();
  }
  return m_instance;
}
