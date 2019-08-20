#include "thumperimageprovider.h"
#include "imagedao.h"

AsyncImageResponse::AsyncImageResponse(qint64 id, const QSize &requestedSize)
  : m_id(id), m_requestedSize(requestedSize)
{
  setAutoDelete(false);
}

QQuickTextureFactory *AsyncImageResponse::textureFactory() const
{
  return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void AsyncImageResponse::run()
{
  if(!m_cancelled) {
    m_image = ImageDao::instance()->requestImage(m_id, m_requestedSize, &m_cancelled);
  }
  emit finished();
}

void AsyncImageResponse::cancel()
{
  m_cancelled = true;
}

ThumperAsyncImageProvider::ThumperAsyncImageProvider()
{

}

QQuickImageResponse *ThumperAsyncImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
  AsyncImageResponse *response = new AsyncImageResponse(id.toLongLong(), requestedSize);
  m_threadPool.start(response);
  return response;
}
