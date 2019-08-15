#include "thumperimageprovider.h"
#include "imagedao.h"

ThumperImageProvider *ThumperImageProvider::m_instance;

ThumperImageProvider::ThumperImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {

}

ThumperImageProvider::~ThumperImageProvider()
{

}

QImage ThumperImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
  return ImageDao::instance()->requestImage(id, size, requestedSize);
}

ThumperImageProvider *ThumperImageProvider::instance() {
  if(m_instance == nullptr) {
    m_instance = new ThumperImageProvider();
  }
  return m_instance;
}
