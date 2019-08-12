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

  QMap<QString, QPixmap> m_data;
  sqlite3 *m_db = nullptr;
public:
  QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
  void insert(const QString &id, QPixmap &pixmap);
  void insert2(const QString &id, const QByteArray &data);
  bool hasKey(const QString &id) { return m_data.contains(id); }

  QVector<QString> loadExistingIds();

  static ThumperImageProvider *instance();
};

#endif // THUMPERIMAGEPROVIDER_H
