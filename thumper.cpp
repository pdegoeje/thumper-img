#include "thumper.h"

Thumper::Thumper(QObject *parent) : QObject(parent)
{

}

QString Thumper::databasePath() const
{
  return QFileInfo(m_databaseFilename).absoluteDir().path();
}

QString Thumper::resolveRelativePath(const QString &path) const {
  QDir pdir(path);
  if(pdir.isAbsolute()) {
    return path;
  }

  QDir dir = databasePath();
  QDir dbRel = dir.filePath(path);
  return dbRel.path();
}

QString Thumper::textTemplate(const QString &blueprint, std::function<QString (const QString &)> tagFunc)
{
  QString output;

  int startIndex = 0;
  while(true) {
    int nextIndex = blueprint.indexOf('$', startIndex);
    if(nextIndex != -1) {
      // start of variable
      output.append(blueprint.mid(startIndex, nextIndex - startIndex));
    } else {
      output.append(blueprint.mid(startIndex));
      break;
    }

    startIndex = nextIndex + 1;
    nextIndex = blueprint.indexOf('$', startIndex);
    if(nextIndex != -1) {
      auto variable = blueprint.mid(startIndex, nextIndex - startIndex);
      output.append(tagFunc(variable));
    } else {
      output.append(blueprint.mid(startIndex));
      break;
    }
    startIndex = nextIndex + 1;
  }

  return output;
}
