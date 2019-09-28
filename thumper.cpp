#include "thumper.h"

Thumper::Thumper(QObject *parent) : QObject(parent)
{

}

QString Thumper::databaseAbsolutePath() const
{
  return QFileInfo(m_databaseFilename).absoluteDir().path();
}

QString Thumper::databaseRelativePath(const QString &path) const {
  QDir pdir(path);
  if(pdir.isAbsolute()) {
    return path;
  }

  QDir dir = databaseAbsolutePath();
  QDir dbRel = dir.filePath(path);
  return dbRel.path();
}
