#include "thumperimageprovider.h"
#include "imagedao.h"

#include <QDebug>
#include <QCache>
#include <QMutex>

static QCache<qint64, QImage> imageCache(250 * 1000 * 1000);
static QSize cachedSize;
static QMutex cacheLock;

static QImage cacheFetch(qint64 id, const QSize &requestedSize) {
  QMutexLocker lock(&cacheLock);
  if(requestedSize == cachedSize) {
    auto ptr = imageCache.object(id);
    if(ptr != nullptr) {
      return *ptr;
    }
  }
  return {};
}

static void cacheInsert(qint64 id, const QSize &requestedSize, const QImage &image) {
  QMutexLocker lock(&cacheLock);
  if(cachedSize != requestedSize) {
    cachedSize = requestedSize;
    imageCache.clear();
  }
  imageCache.insert(id, new QImage(image), image.sizeInBytes());
}

AsyncImageResponse::AsyncImageResponse(qint64 id, const QSize &requestedSize)
  : m_id(id), m_requestedSize(requestedSize)
{
  setAutoDelete(false);
}

QQuickTextureFactory *AsyncImageResponse::textureFactory() const
{
  QMutexLocker lock(&cacheLock);
  if(m_image.isNull()) {
    qDebug() << __FUNCTION__ << "Image is null";
  }
 // qDebug() << __FUNCTION__ << QThread::currentThreadId();
  return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void AsyncImageResponse::run()
{
  bool fromCache = false;
  if(!m_cancelled) {
    //m_image = ImageDao::instance()->requestImage(m_id, m_requestedSize, &m_cancelled);

    fromCache = true;
    m_image = cacheFetch(m_id, m_requestedSize);
    if(m_image.isNull()) {
      fromCache = false;
      m_image = ImageDao::instance()->requestImage(m_id, m_requestedSize, &m_cancelled);
      if(!m_image.isNull()) {
        cacheInsert(m_id, m_requestedSize, m_image);
      }
    }
  }

  /*if(!m_cancelled) {
    m_image = QImage(m_requestedSize.width(), m_requestedSize.height(), QImage::Format_RGB32);
    m_image.fill(QColor::fromHsv(m_id % 360, 127, 127));
  }*/
  //qDebug() << "Finished" << m_id << "Cancelled" << m_cancelled << "Cached" << fromCache;
  emit finished();
}

void AsyncImageResponse::cancel()
{
 // qDebug() << "Cancel" << QThread::currentThreadId();
  m_cancelled = true;
}

QQuickImageResponse *ThumperAsyncImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
  AsyncImageResponse *response = new AsyncImageResponse(id.toLongLong(), requestedSize);  
  //qDebug() << "Load" << id << QThread::currentThreadId();
  m_imageLoadPool.start(response);
  return response;
}
