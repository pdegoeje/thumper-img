#include "imageprocessor.h"

#include <QFile>
#include <QClipboard>
#include <QGuiApplication>

ImageProcessor::ImageProcessor(QObject *parent) : QObject(parent)
{

}

int ImageProcessor::nextId(const QString &prefix) {
  for(int i = 1; i < 1000; i++) {
    QString val = QString::asprintf("%s%d.png", qUtf8Printable(prefix), i);
    if(!QFile::exists(val)) {
      QClipboard *cb = QGuiApplication::clipboard();
      cb->setText(QString::asprintf("%s%d", qUtf8Printable(prefix), i));
      return i;
    }
  }
  return 0;
}
