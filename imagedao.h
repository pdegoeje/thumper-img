#ifndef IMAGEDAO_H
#define IMAGEDAO_H

#include <QObject>
#include <QVariantMap>

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
  Q_INVOKABLE QStringList allIds();
  Q_INVOKABLE QStringList idsByTags(const QStringList &tags);
  Q_INVOKABLE QVariantList tagsByMultipleIds(const QStringList &ids);
  Q_INVOKABLE void addTagToMultipleIds(const QStringList &ids, const QString &tag);
  Q_INVOKABLE void removeTagFromMultipleIds(const QStringList &ids, const QString &tag);
  Q_INVOKABLE QVariantList allTagsCount();

  QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
  void insert(const QString &id, const QByteArray &data);

  static ImageDao *instance();

private:
  void createTemporaryTable(const QString &tableName, const QStringList &items);
  void destroyTemporaryTable(const QString &tableName);

  void createSelectionTable(const QStringList &ids);
  void destroySelectionTable();
signals:

public slots:
};

#endif // IMAGEDAO_H
