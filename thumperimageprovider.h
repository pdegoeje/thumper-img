#ifndef THUMPERIMAGEPROVIDER_H
#define THUMPERIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QThreadPool>
#include <QCache>
#include <QMutex>

class AsyncImageResponse : public QQuickImageResponse, public QRunnable
{
public:
  AsyncImageResponse(qint64 id, const QSize &requestedSize);

  QQuickTextureFactory *textureFactory() const override;

  void run() override;
  void cancel() override;

  volatile bool m_cancelled = false;
  qint64 m_id;
  QSize m_requestedSize;
  QImage m_image;
};

class ThumperAsyncImageProvider : public QQuickAsyncImageProvider {
private:
  QThreadPool m_imageLoadPool;
public:
  QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};

#endif // THUMPERIMAGEPROVIDER_H
