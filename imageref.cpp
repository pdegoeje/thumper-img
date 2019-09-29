#include "imageref.h"
#include "thumper.h"

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
  return Thumper::textTemplate(m_overlayFormat, [&](const QStringRef &tag){
    if(tag == QStringLiteral("id")) {
      return QString::number(m_fileId);
    } else if(tag == QStringLiteral("width")) {
      return QString::number(m_size.width());
    } else if(tag == QStringLiteral("height")) {
      return QString::number(m_size.height());
    } else if(tag == QStringLiteral("size")) {
      return QString::number(m_fileSize / 1000);
    } else if(tag == QStringLiteral("tags")) {
      return tags().join(' ');
    } else if(tag == QStringLiteral("format")) {
      return m_format;
    } else if(tag == QStringLiteral("url")) {
      return m_url;
    } else {
      return QStringLiteral("$%1$").arg(tag);
    }
  });
}
