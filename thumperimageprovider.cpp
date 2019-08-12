#include "thumperimageprovider.h"

#include "sqlite3.h"

ThumperImageProvider *ThumperImageProvider::m_instance;

ThumperImageProvider::ThumperImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {
  if(sqlite3_open("test.db", &m_db) != SQLITE_OK) {
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

QPixmap ThumperImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
  QPixmap pix;
  sqlite3_stmt *stmt = nullptr;

  if(sqlite3_prepare_v2(m_db, "SELECT image FROM store WHERE id = ?1", -1, &stmt, nullptr) != SQLITE_OK)
    goto error;

  if(sqlite3_bind_text(stmt, 1, qUtf8Printable(id.toUtf8()), -1, SQLITE_TRANSIENT) != SQLITE_OK)
    goto error;

  if(sqlite3_step(stmt) != SQLITE_ROW)
    goto error;

  const uchar *data = (const uchar *)sqlite3_column_blob(stmt, 0);
  int bytes = sqlite3_column_bytes(stmt, 0);

  pix.loadFromData(data, bytes);
  if(size) {
    *size = QSize(pix.width(), pix.height());
  }

  sqlite3_finalize(stmt);
  return pix;

error:
  qWarning("SQLite error %d: %s", sqlite3_errcode(m_db), sqlite3_errmsg(m_db));
  sqlite3_finalize(stmt);
  return pix;
}

void ThumperImageProvider::insert(const QString &id, QPixmap &pixmap)
{
  m_data.insert(id, pixmap);
}

void ThumperImageProvider::insert2(const QString &id, const QByteArray &data)
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
