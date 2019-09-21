#ifndef SQLITEHELPER_H
#define SQLITEHELPER_H

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QMutexLocker>

#include "sqlite3.h"

struct sqlite3;

struct SQLiteConnection;

struct SQLitePreparedStatement {
  sqlite3_stmt *m_stmt = nullptr;

  void init(sqlite3 *db, const char *statement);
  void exec(const char *debug_str = nullptr); // step + reset
  void bind(int param, const QString &text);
  void bind(int param, qint64 value);
  void bind(int param, const QByteArray &data);
  QString resultString(int index);
  qint64 resultInteger(int index);
  bool step(const char *debug_str = nullptr);
  void reset();
  void clear();
  void destroy();

  SQLitePreparedStatement() { }
  SQLitePreparedStatement(SQLiteConnection *conn, const char *sql);
  ~SQLitePreparedStatement() { destroy(); }
};

struct SQLiteConnectionPool;

struct SQLiteConnection {
  sqlite3 *m_db;
  SQLiteConnectionPool *m_pool;

  SQLiteConnection(const QString &dbname, int flags, SQLiteConnectionPool *pool);
  ~SQLiteConnection();

  bool exec(const char *sql, const char *debug_str = nullptr);
  QMutex *writeLock();
};

struct SQLiteConnectionPool {
  //int m_maxPool;
  QVector<SQLiteConnection *> m_pool;
  QString m_dbname;
  int m_flags;
  QMutex m_mutex;
  QMutex m_writeLock;

  SQLiteConnectionPool(const QString &dbname, int flags) {
    //m_maxPool = maxPool;
    m_dbname = dbname;
    m_flags = flags;
  }

  ~SQLiteConnectionPool() {
    for(auto conn : m_pool) {
      delete conn;
    }
  }

  QMutex *writeLock() {
    return &m_writeLock;
  };

  SQLiteConnection *open() {
    QMutexLocker lock(&m_mutex);
    if(m_pool.isEmpty()) {
      return new SQLiteConnection(m_dbname, m_flags, this);
    } else {
      auto result = m_pool.last();
      m_pool.removeLast();
      return result;
    }
  }
  void close(SQLiteConnection *conn) {
    QMutexLocker lock(&m_mutex);
    m_pool.append(conn);
  }
};

#endif // SQLITEHELPER_H
