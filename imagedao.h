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

struct RawImageQuery {
  SQLitePreparedStatement ps;
  QByteArray data;

  RawImageQuery(const SQLiteConnection &conn, qint64 id) : ps(conn.prepare("SELECT image FROM store WHERE id = ?1")) {
    ps.bind(1, id);
    ps.step();
    data = ps.resultBlobPointer(0);
  }

  QImage decode(const QSize &size = {});
};

struct ImageRenderContext {
  QList<QObject *> irefs;
  QString path;
  QSize size;
  int flags;
};

class ImageDaoDeferredWriter : public QObject {
  Q_OBJECT

  void startWrite();
  void startBusy();

  SQLiteConnection m_conn;
  bool m_inTransaction = false;
  bool m_busy = false;
public:
  ImageDaoDeferredWriter(SQLiteConnection &&conn, QObject *parent = nullptr);
  virtual ~ImageDaoDeferredWriter();
private slots:
  void endWrite();
  void endBusy();
public slots:  

  void backgroundTask(const QString &name);

  void addTag(const QList<QObject *> &irefs, const QString &tag);
  void removeTag(const QList<QObject *> &irefs, const QString &tag);
  void updateDeleted(const QList<QObject *> &irefs, bool deleted);
  void compressImages(const QList<QObject *> &irefs);
  void writeImage(const QUrl &url, const QByteArray &data);
  void renderImages(const ImageRenderContext &ric);

  void task_fixImageMetaData();
  void task_purgeDeletedImages();
  void task_vacuum();
signals:
  void updateImageData(ImageRef *update, const QString &newFormat, qint64 newFileSize, QImage::Format newPixelFormat);
  void writeComplete(const QUrl &url, quint64 fileId);
  void busyChanged(bool busyState);
};


Q_DECLARE_METATYPE(ImageRenderContext)

class ImageDao : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

  static ImageDao *m_instance;
  static QString m_databaseFilename;

  SQLiteConnectionPool m_connPool;
  SQLiteConnection m_conn;

  QThread m_writeThread;
  QMap<quint64, ImageRef *> m_refMap;
  QReadWriteLock m_refMapLock;
  QImage makeThumbnail(SQLiteConnection *conn, ImageRef *iref, int thumbsize);

  bool m_busy = false;
public:
  enum RenderFlags {
    PAD_TO_FIT = 0x01,
    FNAME_TO_CLIPBOARD = 0x2,
  };

  Q_ENUM(RenderFlags)

  explicit ImageDao(QObject *parent = nullptr);
  virtual ~ImageDao();

  SQLiteConnectionPool *connPool() { return &m_connPool; }

  bool tableExists(const QString &table);

  static void setDatabaseFilename(const QString &filename);

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


  Q_INVOKABLE QList<QObject *> updateDeleted(const QList<QObject *> &irefs, bool deleted);

  Q_INVOKABLE void renderImages(const QList<QObject *> &irefs, const QString &path, int requestedSize, int flags);

  Q_INVOKABLE void backgroundTask(const QString &name);

  QImage requestImage(qint64 id, const QSize &requestedSize, volatile bool *cancelled);

  static QString imageHash(const QByteArray &data);
  static ImageDao *instance();

  bool busy() const { return m_busy; }
public slots:
  void setBusy(bool busyState);
  void updateImageData(ImageRef *update, const QString &newFormat, qint64 newFileSize, QImage::Format newPixelFormat);
signals:
  void deferredBackgroundTask(const QString &name);
  void deferredUpdateDeleted(const QList<QObject *> &irefs, bool deleted);
  void deferredAddTag(const QList<QObject *> &irefs, const QString &tag);
  void deferredRemoveTag(const QList<QObject *> &irefs, const QString &tag);
  void deferredCompressImages(const QList<QObject *> &irefs);
  void deferredRenderImages(const ImageRenderContext &ric);

  void deferredWriteImage(const QUrl &url, const QByteArray &data);  
  void writeComplete(const QUrl &url, qint64 id);

  void busyChanged();
public slots:
};

#endif // IMAGEDAO_H
