#include "thumperimageprovider.h"
#include "imagedao.h"

AsyncImageResponse::AsyncImageResponse(qint64 id, const QSize &requestedSize)
  : m_id(id), m_requestedSize(requestedSize)
{
  setAutoDelete(false);
}

QQuickTextureFactory *AsyncImageResponse::textureFactory() const
{
  //qDebug() << "LoadTexture" << m_id << QThread::currentThreadId();
  return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void AsyncImageResponse::run()
{
  if(!m_cancelled) {
    m_image = ImageDao::instance()->requestImage(m_id, m_requestedSize, &m_cancelled);
  }

//  qDebug() << "Finished" << m_id << receivers(SIGNAL(finished()));
  QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
  //emit finished();
}

void AsyncImageResponse::cancel()
{
  m_cancelled = true;
}

QQuickImageResponse *ThumperAsyncImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
  //qDebug() << "Loading" << id.toLongLong() << QThread::currentThreadId();

  AsyncImageResponse *response = new AsyncImageResponse(id.toLongLong(), requestedSize);  
  m_imageLoadPool.start(response);

  //QThread::usleep(1000);
  return response;
}
