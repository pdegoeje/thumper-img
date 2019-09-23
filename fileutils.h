#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QObject>
#include <QFileDialog>

class FileUtils : public QObject
{
  Q_OBJECT

  QFileDialog m_fileDialog;
public:
  explicit FileUtils(QObject *parent = nullptr);

  Q_INVOKABLE void save(const QString &path, const QString &data);
  Q_INVOKABLE QString load(const QString &path);
  Q_INVOKABLE void openImageDatabase();
signals:
  void imageDatabaseSelected(const QString &file);
public slots:
};

#endif // FILEUTILS_H
