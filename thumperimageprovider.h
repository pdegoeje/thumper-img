#ifndef THUMPERIMAGEPROVIDER_H
#define THUMPERIMAGEPROVIDER_H

#include <QQuickImageProvider>

class ThumperImageProvider : public QQuickImageProvider
{
  static ThumperImageProvider *m_instance;

  ThumperImageProvider();
  virtual ~ThumperImageProvider();
public:
  QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
  static ThumperImageProvider *instance();
};

#endif // THUMPERIMAGEPROVIDER_H
