#ifndef SQLITEHELPER_H
#define SQLITEHELPER_H

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QMutex>

struct sqlite3;
struct sqlite3_stmt;

struct SQLiteConnection;

#define DBG_STRINGIFY(x) #x
#define DBG_TOSTRING(x) DBG_STRINGIFY(x)

#ifdef _WIN32
#define SRC_LOCATION __FUNCTION__ "#" DBG_TOSTRING(__LINE__)
#else
#define SRC_LOCATION __FILE__ "#" DBG_TOSTRING(__LINE__)
#endif

struct SQLitePreparedStatement {
  sqlite3_stmt *m_stmt = nullptr;

  void init(sqlite3 *db, const char *statement);
  void init(SQLiteConnection *conn, const char *statement);
  void exec(const char *debug_str = nullptr); // step + reset
  void bind(int param, const QString &text);
  void bind(int param, qint64 value);
  void bind(int param, const QByteArray &data);
  QString resultString(int index);
  qint64 resultInteger(int index);
  QByteArray resultBlobPointer(int index);
  bool step(const char *debug_str = nullptr);
  void reset();
  void clear();
  void destroy();

  SQLitePreparedStatement() { }
  SQLitePreparedStatement(const SQLitePreparedStatement &) = delete;
  SQLitePreparedStatement(sqlite3 *m_db, const char *sql);
  ~SQLitePreparedStatement() { destroy(); }
};

struct SQLiteConnectionPool;

struct SQLiteConnection {
  sqlite3 *m_db = nullptr;
  SQLiteConnectionPool *m_pool = nullptr;

  SQLitePreparedStatement prepare(const char *sql) {
    return { m_db, sql };
  }

  bool exec(const char *sql, const char *debug_str = nullptr);
  QMutex *writeLock();

  SQLiteConnection() { };
  SQLiteConnection(sqlite3 *conn, SQLiteConnectionPool *pool) : m_db(conn), m_pool(pool) { }
  SQLiteConnection(const SQLiteConnection &) = delete;
  SQLiteConnection(SQLiteConnection &&other);
  ~SQLiteConnection();

  void operator = (SQLiteConnection &&other);
};

struct SQLiteConnectionPool {
  QVector<sqlite3 *> m_pool;
  QString m_dbname;
  int m_flags;
  QMutex m_mutex;
  QMutex m_writeLock;

  SQLiteConnectionPool(const QString &dbname, int flags);
  ~SQLiteConnectionPool();

  QMutex *writeLock() {
    return &m_writeLock;
  }

  SQLiteConnection open();
  void close(sqlite3 *conn);
};

#endif // SQLITEHELPER_H
