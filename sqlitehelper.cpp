#include "sqlitehelper.h"
#include "sqlite3.h"

#include <QDebug>
#include <QMutexLocker>

void SQLitePreparedStatement::init(sqlite3 *db, const char *statement)
{
  if(sqlite3_prepare_v2(db, statement, -1, &m_stmt, nullptr) != SQLITE_OK) {
    qWarning("Failed to prepare statement: %s", sqlite3_errmsg(db));
  }
}

void SQLitePreparedStatement::init(SQLiteConnection *conn, const char *statement)
{
  init(conn->m_db, statement);
}

void SQLitePreparedStatement::exec(const char *debug_str)
{
  step(debug_str);
  reset();
}

void SQLitePreparedStatement::bind(int param, const QString &text)
{
  if(sqlite3_bind_text16(m_stmt, param, text.constData(), text.size() * 2, SQLITE_TRANSIENT) != SQLITE_OK) {
    qWarning("SQLite bind text error: %s", sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
  }
}

void SQLitePreparedStatement::bind(int param, qint64 value)
{
  if(sqlite3_bind_int64(m_stmt, param, value) != SQLITE_OK) {
    qWarning("SQLite bind integer error: %s", sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
  }
}

void SQLitePreparedStatement::bind(int param, const QByteArray &data)
{
  if(sqlite3_bind_blob(m_stmt, param, data.data(), data.length(), SQLITE_TRANSIENT) != SQLITE_OK) {
    qWarning("SQLite bind blob error: %s", sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
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

QByteArray SQLitePreparedStatement::resultBlobPointer(int index)
{
  const char *data = (const char *)sqlite3_column_blob(m_stmt, index);
  if(data != nullptr) {
    int bytes = sqlite3_column_bytes(m_stmt, index);
    return QByteArray::fromRawData(data, bytes);
  }
  return {};
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

  qWarning("%s: SQLite step error: %s", debug_str, sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
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

SQLiteConnection::~SQLiteConnection()
{
  if(m_pool && m_db) {
    m_pool->close(m_db);
  }
}

void SQLiteConnection::operator =(SQLiteConnection &&other)
{
  this->m_db = other.m_db;
  this->m_pool = other.m_pool;

  other.m_db = nullptr;
  other.m_pool = nullptr;
}

bool SQLiteConnection::exec(const char *sql, const char *debug_str)
{
  char *errmsg;
  if(sqlite3_exec(m_db, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
    qWarning("%s: SQLite exec error: %s", debug_str, errmsg);
    return false;
  }

  return true;
}

QMutex *SQLiteConnection::writeLock() {
  return m_pool->writeLock();
}

SQLiteConnection::SQLiteConnection(SQLiteConnection &&other)
{
  this->m_db = other.m_db;
  this->m_pool = other.m_pool;

  other.m_db = nullptr;
  other.m_pool = nullptr;
}

SQLiteConnectionPool::SQLiteConnectionPool(const QString &dbname, int flags) {
  m_dbname = dbname;
  m_flags = flags;
}

SQLiteConnectionPool::~SQLiteConnectionPool() {
  for(auto conn : m_pool) {
    sqlite3_close_v2(conn);
    qInfo("Closed database connection");
  }
  qDebug() << SRC_LOCATION << "All connections closed";
}

SQLiteConnection SQLiteConnectionPool::open()
{
  QMutexLocker lock(&m_mutex);
  if(m_pool.isEmpty()) {
    sqlite3 *db = nullptr;
    if(sqlite3_open_v2(qUtf8Printable(m_dbname), &db, m_flags, nullptr) != SQLITE_OK) {
      qWarning("Couldn't open SQLite database: %s", sqlite3_errmsg(db));
    } else {
      qInfo("Created new connection to %s", qUtf8Printable(m_dbname));
    }
    return { db, this };
  } else {
    auto db = m_pool.last();
    m_pool.removeLast();
    return { db, this };
  }
}

void SQLiteConnectionPool::close(sqlite3 *conn) {
  QMutexLocker lock(&m_mutex);
  m_pool.append(conn);
}

