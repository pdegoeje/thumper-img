#ifndef THUMPERIMAGEPROVIDER_H
#define THUMPERIMAGEPROVIDER_H

#include <QQuickImageProvider>

class ThumperImageProvider : public QQuickImageProvider
{
public:
  ThumperImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) { }

  QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};

#endif // THUMPERIMAGEPROVIDER_H
