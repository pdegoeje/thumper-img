#ifndef IMAGEDAO_H
#define IMAGEDAO_H

#include "sqlitehelper.h"

#include <QObject>
#include <QVariantMap>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QSize>
#include <QThread>
#include <QTimer>

class ImageRef : public QObject {
  Q_OBJECT

  Q_PROPERTY(qint64 fileId MEMBER m_fileId CONSTANT)
  Q_PROPERTY(bool selected MEMBER m_selected NOTIFY selectedChanged)
  Q_PROPERTY(QStringList tags READ tags NOTIFY tagsChanged)
  Q_PROPERTY(QSize size READ size CONSTANT)
  Q_PROPERTY(bool deleted MEMBER m_deleted NOTIFY deletedChanged)

  qint64 m_fileId;
  bool m_selected;
  bool m_deleted;
  QSet<QString> m_tags;
  QSize m_size;
  qint64 m_phash;
public:
  QStringList tags();
  QSize size() const { return m_size; }
signals:
  void selectedChanged();
  void tagsChanged();
  void deletedChanged();

  friend class ImageDao;
  friend class ImageDaoDeferredWriter;
};

class ImageProcessStatus : public QObject {
  Q_OBJECT
public:
signals:
  void update(const QString &status, qreal fractionComplete);
  void complete();
};

class ImageDaoSyncPoint: public QObject {
  Q_OBJECT
public:
signals:
  void sync(const QVariant &userData);
};

class ImageDaoDeferredWriter : public QObject {
  Q_OBJECT

  void startTransaction();
  void endTransaction();

  void startWrite();

  SQLiteConnection *m_conn = nullptr;
  bool m_inTransaction = false;
public:
  ImageDaoDeferredWriter(SQLiteConnection *conn, QObject *parent = nullptr);
private slots:
  void endWrite();
public slots:
  //void writeImage();
  void addTag(const QList<QObject *> &irefs, const QString &tag);
  void removeTag(const QList<QObject *> &irefs, const QString &tag);
  void updateDeleted(const QList<QObject *> &irefs, bool deleted);
  void sync(ImageDaoSyncPoint *syncPoint, const QVariant &userData);

  void writeImage(const QUrl &url, const QByteArray &data);
signals:
  void writeComplete(const QUrl &url, const QString &hash);
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
  QThread m_writeThread;
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

  Q_INVOKABLE QList<QObject *> addTag(const QList<QObject *> &irefs, const QString &tag);
  Q_INVOKABLE QList<QObject *> removeTag(const QList<QObject *> &irefs, const QString &tag);
  Q_INVOKABLE QList<QObject *> findAllDuplicates(const QList<QObject *> &irefs, int maxDuplicates = 5);
  Q_INVOKABLE QString hashById(qint64 id);
  Q_INVOKABLE QVariantList tagCount(const QList<QObject *> &irefs);
  Q_INVOKABLE QList<QObject *> search(const QList<QObject *> &irefs, const QStringList &tags);
  Q_INVOKABLE QList<QObject *> all(bool includeDeleted);
  Q_INVOKABLE QStringList tagsById(qint64 id);
  void addTag(qint64 fileId, const QString &tag);
  void removeTag(qint64 fileId, const QString &tag);
  Q_INVOKABLE ImageRef *findHash(const QString &hash);

  Q_INVOKABLE void purgeDeletedImages();
  Q_INVOKABLE QList<QObject *> updateDeleted(const QList<QObject *> &irefs, bool deleted);

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
signals:
  void deferredUpdateDeleted(const QList<QObject *> &irefs, bool deleted);
  void deferredAddTag(const QList<QObject *> &irefs, const QString &tag);
  void deferredRemoveTag(const QList<QObject *> &irefs, const QString &tag);
  void deferredSync(ImageDaoSyncPoint *syncPoint, const QVariant &userData);

  void deferredWriteImage(const QUrl &url, const QByteArray &data);
  void writeComplete(const QUrl &url, const QString &hash);
public slots:
};

#endif // IMAGEDAO_H
