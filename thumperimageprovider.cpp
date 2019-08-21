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
  if(m_image.isNull()) {
    qDebug() << __FUNCTION__ << "Image is null";
  }
  return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void AsyncImageResponse::run()
{
  if(!m_cancelled) {    
    //m_image = ImageDao::instance()->requestImage(m_id, m_requestedSize, &m_cancelled);

    m_image = cacheFetch(m_id, m_requestedSize);
    if(m_image.isNull()) {
      m_image = ImageDao::instance()->requestImage(m_id, m_requestedSize, &m_cancelled);
      if(!m_image.isNull()) {
        cacheInsert(m_id, m_requestedSize, m_image);
      }
    }
  }
  emit finished();
}

void AsyncImageResponse::cancel()
{
  m_cancelled = true;
}

QQuickImageResponse *ThumperAsyncImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
  AsyncImageResponse *response = new AsyncImageResponse(id.toLongLong(), requestedSize);  
  m_imageLoadPool.start(response);
  return response;
}
