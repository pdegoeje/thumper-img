#ifndef IMAGEDAO_H
#define IMAGEDAO_H

#include <QObject>
#include <QVariantMap>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>

#include "sqlite3.h"

struct sqlite3;

struct SQLitePreparedStatement {
  sqlite3_stmt *m_stmt = nullptr;

  void init(sqlite3 *db, const char *statement);
  void exec(); // step + reset
  void bind(int param, const QString &text);
  void bind(int param, qint64 value);
  void bind(int param, const QByteArray &data);
  QString resultString(int index);
  qint64 resultInteger(int index);
  bool step();
  void reset();
  void clear();
  void destroy();

  ~SQLitePreparedStatement() { destroy(); }
};

struct SQLiteConnection {
  sqlite3 *m_db;

  SQLiteConnection(const QString &dbname, int flags);
  ~SQLiteConnection();
};

struct SQLiteConnectionPool {
  //int m_maxPool;
  QVector<SQLiteConnection *> m_pool;
  QString m_dbname;
  int m_flags;
  QMutex m_mutex;

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

  SQLiteConnection *open() {
    QMutexLocker lock(&m_mutex);
    if(m_pool.isEmpty()) {
      return new SQLiteConnection(m_dbname, m_flags);
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

class ImageRef : public QObject {
  Q_OBJECT

  Q_PROPERTY(qint64 fileId MEMBER m_fileId CONSTANT)
  Q_PROPERTY(bool selected MEMBER m_selected NOTIFY selectedChanged)
  Q_PROPERTY(QStringList tags READ tags NOTIFY tagsChanged)

  qint64 m_fileId;
  bool m_selected;
  QSet<QString> m_tags;
public:
  QStringList tags();
signals:
  void selectedChanged();
  void tagsChanged();

  friend class ImageDao;
};


class ImageDao : public QObject
{
  Q_OBJECT

  static ImageDao *m_instance;

  SQLiteConnectionPool m_connPool;
  SQLiteConnection *m_conn;

  sqlite3 *m_db = nullptr;

  SQLitePreparedStatement m_ps_insert;
  SQLitePreparedStatement m_ps_addTag;
  SQLitePreparedStatement m_ps_removeTag;
  SQLitePreparedStatement m_ps_tagsById;
  SQLitePreparedStatement m_ps_idByHash;
  SQLitePreparedStatement m_ps_search;
  SQLitePreparedStatement m_ps_all;
  //SQLitePreparedStatement m_ps_imageById;
  SQLitePreparedStatement m_ps_transStart;
  SQLitePreparedStatement m_ps_transEnd;
public:
  explicit ImageDao(QObject *parent = nullptr);
  virtual ~ImageDao();

  Q_INVOKABLE void addTag(ImageRef *iref, const QString &tag);
  Q_INVOKABLE void removeTag(ImageRef *iref, const QString &tag);

  Q_INVOKABLE QVariantList tagCount(const QList<QObject *> &irefs);
  Q_INVOKABLE QList<QObject *> searchSubset(const QList<QObject *> &irefs, const QStringList &tags);
  Q_INVOKABLE QList<QObject *> search(const QStringList &tags);
  Q_INVOKABLE QList<QObject *> all();
  Q_INVOKABLE QStringList tagsById(qint64 id);
  void addTag(qint64 fileId, const QString &tag);
  void removeTag(qint64 fileId, const QString &tag);
  Q_INVOKABLE ImageRef *findHash(const QString &hash);

  Q_INVOKABLE void transactionStart();
  Q_INVOKABLE void transactionEnd();

  QImage requestImage(qint64 id, const QSize &requestedSize, volatile bool *cancelled);
  void insert(const QString &hash, const QByteArray &data);

  static ImageDao *instance();
private:
  void createTemporaryTable(const QString &tableName, const QStringList &items);
  void destroyTemporaryTable(const QString &tableName);
signals:
public slots:
};

#endif // IMAGEDAO_H
