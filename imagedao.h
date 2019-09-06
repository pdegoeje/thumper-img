#ifndef IMAGEDAO_H
#define IMAGEDAO_H

#include <QObject>
#include <QVariantMap>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QSize>

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

struct SQLiteConnection {
  sqlite3 *m_db;

  SQLiteConnection(const QString &dbname, int flags);
  ~SQLiteConnection();

  bool exec(const char *sql, const char *debug_str = nullptr);
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
  Q_PROPERTY(QSize size READ size CONSTANT)

  qint64 m_fileId;
  bool m_selected;
  QSet<QString> m_tags;
  QSize m_size;
  qint64 m_phash;
public:
  QStringList tags();
  QSize size() const { return m_size; }
signals:
  void selectedChanged();
  void tagsChanged();

  friend class ImageDao;
};

class ImageProcessStatus : public QObject {
  Q_OBJECT
public:
signals:
  void update(const QString &status, qreal fractionComplete);
  void complete();
};


class ImageDao : public QObject
{
  Q_OBJECT

  static ImageDao *m_instance;

  SQLiteConnectionPool m_connPool;
  SQLiteConnection *m_conn;

  sqlite3 *m_db = nullptr;

  SQLitePreparedStatement m_ps_addTag;
  SQLitePreparedStatement m_ps_removeTag;
  SQLitePreparedStatement m_ps_tagsById;
  SQLitePreparedStatement m_ps_idByHash;
  SQLitePreparedStatement m_ps_transStart;
  SQLitePreparedStatement m_ps_transEnd;

  QElapsedTimer m_timer;
  QMutex m_writeLock;
public:
  struct ImageDataContext {
    QByteArray data;
    SQLiteConnection *conn;
    SQLitePreparedStatement ps;
  };

  enum RenderFlags {
    PAD_TO_FIT = 0x01,
    FNAME_TO_CLIPBOARD = 0x2,
  };

  Q_ENUM(RenderFlags)

  explicit ImageDao(QObject *parent = nullptr);
  virtual ~ImageDao();

  SQLiteConnectionPool *connPool() { return &m_connPool; }

  bool tableExists(const QString &table);
  Q_INVOKABLE void metaPut(const QString &key, const QVariant &val);
  Q_INVOKABLE QVariant metaGet(const QString &key);

  Q_INVOKABLE bool addTag(ImageRef *iref, const QString &tag);
  Q_INVOKABLE bool removeTag(ImageRef *iref, const QString &tag);

  Q_INVOKABLE QList<QObject *> addTagMultiple(const QList<QObject *> &irefs, const QString &tag);
  Q_INVOKABLE QList<QObject *> removeTagMultiple(const QList<QObject *> &irefs, const QString &tag);
  Q_INVOKABLE QList<QObject *> findAllDuplicates(const QList<QObject *> &irefs);
  Q_INVOKABLE QString hashById(qint64 id);
  Q_INVOKABLE QVariantList tagCount(const QList<QObject *> &irefs);
  Q_INVOKABLE QList<QObject *> search(const QList<QObject *> &irefs, const QStringList &tags);
  Q_INVOKABLE QList<QObject *> all();
  Q_INVOKABLE QStringList tagsById(qint64 id);
  void addTag(qint64 fileId, const QString &tag);
  void removeTag(qint64 fileId, const QString &tag);
  Q_INVOKABLE ImageRef *findHash(const QString &hash);

  void imageDataAcquire(ImageDataContext &idc, qint64 id);
  void imageDataRelease(ImageDataContext &idc);

  Q_INVOKABLE void renderImages(const QList<QObject *> &irefs, const QString &path, int requestedSize, int flags);

  Q_INVOKABLE void transactionStart();
  Q_INVOKABLE void transactionEnd();

  Q_INVOKABLE void lockWrite();
  Q_INVOKABLE void unlockWrite();

  Q_INVOKABLE void timerStart() { m_timer.start(); }
  Q_INVOKABLE qint64 timerElapsed() { return m_timer.elapsed(); }

  Q_INVOKABLE void fixImageMetaData(ImageProcessStatus *status);

  QImage requestImage(qint64 id, const QSize &requestedSize, volatile bool *cancelled);

  static ImageDao *instance();
private:
  void createTemporaryTable(const QString &tableName, const QStringList &items);
signals:
public slots:
};

#endif // IMAGEDAO_H
