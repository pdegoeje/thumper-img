#ifndef IMAGEDAO_H
#define IMAGEDAO_H

#include <QObject>

struct sqlite3;

class ImageDao : public QObject
{
  Q_OBJECT

  static ImageDao *m_instance;

  sqlite3 *m_db = nullptr;
public:
  explicit ImageDao(QObject *parent = nullptr);
  virtual ~ImageDao();

  Q_INVOKABLE void addTag(const QString &id, const QString &tag);
  Q_INVOKABLE void removeTag(const QString &id, const QString &tag);
  Q_INVOKABLE QStringList tagsById(const QString &id);
  Q_INVOKABLE QStringList allTags();
  QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
  void insert(const QString &id, const QByteArray &data);
  Q_INVOKABLE QStringList loadExistingIds();

  static ImageDao *instance();
signals:

public slots:
};

#endif // IMAGEDAO_H
