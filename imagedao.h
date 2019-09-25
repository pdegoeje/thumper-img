#ifndef IMAGEDAO_H
#define IMAGEDAO_H

#include "sqlitehelper.h"
#include "imagemetadata.h"
#include "imageref.h"

#include <QObject>
#include <QVariantMap>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <QReadWriteLock>

class ImageDaoSyncPoint: public QObject {
  Q_OBJECT
public:
signals:
  void sync(const QVariant &userData);
};

class ImageDaoDeferredWriter : public QObject {
  Q_OBJECT

  void startWrite();

  SQLiteConnection m_conn;
  bool m_inTransaction = false;
public:
  ImageDaoDeferredWriter(SQLiteConnection &&conn, QObject *parent = nullptr);
  virtual ~ImageDaoDeferredWriter();
private slots:
  void endWrite();
public slots:
  void addTag(const QList<QObject *> &irefs, const QString &tag);
  void removeTag(const QList<QObject *> &irefs, const QString &tag);
  void updateDeleted(const QList<QObject *> &irefs, bool deleted);
  void sync(ImageDaoSyncPoint *syncPoint, const QVariant &userData);

  void writeImage(const QUrl &url, const QByteArray &data);
signals:
  void writeComplete(const QUrl &url, quint64 fileId);
};


class ImageDao : public QObject
{
  Q_OBJECT

  static ImageDao *m_instance;

  SQLiteConnectionPool m_connPool;
  SQLiteConnection m_conn;

  QThread m_writeThread;
  QMap<quint64, ImageRef *> m_refMap;
  QReadWriteLock m_refMapLock;
  QImage makeThumbnail(SQLiteConnection *conn, ImageRef *iref, int thumbsize);
public:
  struct ImageDataContext {
    QByteArray data;
    SQLiteConnection conn;
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
  Q_INVOKABLE QVariantList tagCount(const QList<QObject *> &irefs);
  Q_INVOKABLE QList<QObject *> search(const QList<QObject *> &irefs, const QStringList &tags);
  Q_INVOKABLE QList<QObject *> all(bool includeDeleted);
  Q_INVOKABLE QStringList tagsById(qint64 id);
  Q_INVOKABLE ImageRef *createImageRef(qint64 id);
  Q_INVOKABLE void compressImages(const QList<QObject *> &irefs);

  Q_INVOKABLE void purgeDeletedImages();
  Q_INVOKABLE QList<QObject *> updateDeleted(const QList<QObject *> &irefs, bool deleted);

  void imageDataAcquire(ImageDataContext &idc, qint64 id);
  void imageDataRelease(ImageDataContext &idc);

  Q_INVOKABLE void renderImages(const QList<QObject *> &irefs, const QString &path, int requestedSize, int flags);
  Q_INVOKABLE void fixImageMetaData(ImageProcessStatus *status);

  QImage requestImage(qint64 id, const QSize &requestedSize, volatile bool *cancelled);

  static QString imageHash(const QByteArray &data);
  static ImageDao *instance();
signals:
  void deferredUpdateDeleted(const QList<QObject *> &irefs, bool deleted);
  void deferredAddTag(const QList<QObject *> &irefs, const QString &tag);
  void deferredRemoveTag(const QList<QObject *> &irefs, const QString &tag);
  void deferredSync(ImageDaoSyncPoint *syncPoint, const QVariant &userData);

  void deferredWriteImage(const QUrl &url, const QByteArray &data);
  void writeComplete(const QUrl &url, qint64 id);
public slots:
};

#endif // IMAGEDAO_H
