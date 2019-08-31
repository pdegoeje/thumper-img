#include "fileutils.h"

#include <QTextStream>
#include <QFile>
#include <QDebug>

FileUtils::FileUtils(QObject *parent) : QObject(parent)
{
}

void FileUtils::save(const QString &path, const QString &data)
{
  QFile file(path);
  if(!file.open(QFile::WriteOnly | QFile::Truncate)) {
    qDebug() << __FUNCTION__ << "Failed to open file" << path;
    return;
  }

  file.write(data.toUtf8());
  file.close();
}

QString FileUtils::load(const QString &path)
{
  QFile file(path);
  if(!file.open(QFile::ReadOnly)) {
    qDebug() << __FUNCTION__ << "Failed to open file" << path;
    return {};
  }

  return QString::fromUtf8(file.readAll());
  file.close();
}
