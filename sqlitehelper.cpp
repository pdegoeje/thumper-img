#include "sqlitehelper.h"

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


SQLiteConnection::SQLiteConnection(const QString &dbname, int flags, SQLiteConnectionPool *pool) : m_pool(pool)
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

QMutex *SQLiteConnection::writeLock() {
  return m_pool->writeLock();
}
