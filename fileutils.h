#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QObject>

class FileUtils : public QObject
{
  Q_OBJECT
public:
  explicit FileUtils(QObject *parent = nullptr);

  Q_INVOKABLE void save(const QString &path, const QString &data);
  Q_INVOKABLE QString load(const QString &path);
signals:

public slots:
};

#endif // FILEUTILS_H
