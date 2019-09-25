#include "imageref.h"

ImageRef::ImageRef(QObject *parent) : QObject(parent) {
  connect(this, &ImageRef::overlayFormatChanged, this, &ImageRef::overlayStringChanged);
  connect(this, &ImageRef::tagsChanged, this, &ImageRef::overlayStringChanged);
}

QStringList ImageRef::tags() const
{
  return m_tags.toList();
}

QString ImageRef::overlayString() const
{
  QString output;

  int startIndex = 0;
  while(true) {
    int nextIndex = m_overlayFormat.indexOf('$', startIndex);
    if(nextIndex != -1) {
      // start of variable
      output.append(m_overlayFormat.midRef(startIndex, nextIndex - startIndex));
    } else {
      output.append(m_overlayFormat.midRef(startIndex));
      break;
    }

    startIndex = nextIndex + 1;
    nextIndex = m_overlayFormat.indexOf('$', startIndex);
    if(nextIndex != -1) {
      auto variable = m_overlayFormat.midRef(startIndex, nextIndex - startIndex);
      if(variable == QStringLiteral("id")) {
        output.append(QString::number(m_fileId));
      } else if(variable == QStringLiteral("width")) {
        output.append(QString::number(m_size.width()));
      } else if(variable == QStringLiteral("height")) {
        output.append(QString::number(m_size.height()));
      } else if(variable == QStringLiteral("size")) {
        output.append(QString::number(m_fileSize / 1000));
      } else if(variable == QStringLiteral("tags")) {
        output.append(tags().join(' '));
      } else if(variable == QStringLiteral("format")) {
        output.append(m_format);
      } else if(variable == QStringLiteral("url")) {
        output.append(m_url);
      } else {
        output.append(QStringLiteral("?%1?").arg(variable));
      }
    } else {
      output.append(m_overlayFormat.midRef(startIndex));
      break;
    }
    startIndex = nextIndex + 1;
  }

  return output;
}
