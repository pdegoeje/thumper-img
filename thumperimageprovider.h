#ifndef THUMPERIMAGEPROVIDER_H
#define THUMPERIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QMap>

struct sqlite3;

class ThumperImageProvider : public QObject, public QQuickImageProvider
{
  Q_OBJECT

  static ThumperImageProvider *m_instance;

  ThumperImageProvider();
  virtual ~ThumperImageProvider();

  sqlite3 *m_db = nullptr;
public:
  QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
  void insert(const QString &id, const QByteArray &data);

  QVector<QString> loadExistingIds();

  static ThumperImageProvider *instance();

  // QQmlImageProviderBase interface
public:
  ImageType imageType() const override { return ImageType::Image; }
};

#endif // THUMPERIMAGEPROVIDER_H
