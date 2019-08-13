#include "imageprocessor.h"
#include "thumperimageprovider.h"

#include <QFile>
#include <QClipboard>
#include <QGuiApplication>
#include <QNetworkReply>
#include <QPixmap>
#include <QImageReader>
#include <QDir>

ImageProcessor::ImageProcessor(QObject *parent) : QObject(parent)
{
  connect(&manager, SIGNAL(finished(QNetworkReply*)),
          SLOT(downloadFinished(QNetworkReply*)));
}

void ImageProcessor::setClipBoard(const QString &data) {
  QClipboard *cb = QGuiApplication::clipboard();
  cb->setText(data);
}

void ImageProcessor::sslErrors(const QList<QSslError> &sslErrors)
{
#if QT_CONFIG(ssl)
  for (const QSslError &error : sslErrors)
    fprintf(stderr, "SSL error: %s\n", qPrintable(error.errorString()));
#else
  Q_UNUSED(sslErrors);
#endif
}


void ImageProcessor::download(const QUrl &url) {
  QNetworkRequest req(url);
  QNetworkReply *reply = manager.get(req);

#if QT_CONFIG(ssl)
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
          SLOT(sslErrors(QList<QSslError>)));
#endif
}

void ImageProcessor::loadExisting()
{
  ThumperImageProvider *tip = ThumperImageProvider::instance();

  auto idList = tip->loadExistingIds();
  for(const auto &id : idList) {
    emit imageReady(id);
  }
}

bool ImageProcessor::saveToDisk(QIODevice *data)
{
  QByteArray bytes = data->readAll();
  QByteArray hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
  QString key(hash.toHex());

  ThumperImageProvider *tip = ThumperImageProvider::instance();

  tip->insert(key, bytes);
  emit imageReady(key);

  return true;
}

void ImageProcessor::downloadFinished(QNetworkReply *reply) {
  QUrl url = reply->url();
  if (reply->error()) {
    qWarning("Download of %s failed: %s",
             url.toEncoded().constData(),
             qPrintable(reply->errorString()));
  } else {
    if (isHttpRedirect(reply)) {
      qInfo("Request was redirected.");
    } else {
      qInfo("Content-Disposition: %s",
            qUtf8Printable(reply->header(QNetworkRequest::ContentDispositionHeader).toString()));

      if (saveToDisk(reply)) {
        qInfo("Download of %s succeeded",
              url.toEncoded().constData());
      }
    }
  }

  reply->deleteLater();
}

bool ImageProcessor::isHttpRedirect(QNetworkReply *reply)
{
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  return statusCode == 301 || statusCode == 302 || statusCode == 303
      || statusCode == 305 || statusCode == 307 || statusCode == 308;
}
