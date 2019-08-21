#include "imageprocessor.h"
#include "imagedao.h"

#include <QFile>
#include <QClipboard>
#include <QGuiApplication>
#include <QNetworkReply>
#include <QPixmap>
#include <QImageReader>
#include <QDir>

ImageProcessor::ImageProcessor(QObject *parent) : QObject(parent)
{
  ImageProcessorWorker *worker = new ImageProcessorWorker();
  worker->moveToThread(&m_workerThread);

  connect(&m_workerThread, &QThread::finished, worker, &QObject::deleteLater);
  connect(this, &ImageProcessor::startDownload, worker, &ImageProcessorWorker::startDownload);
  connect(worker, &ImageProcessorWorker::ready, this, &ImageProcessor::ready);

  m_workerThread.start();
}

ImageProcessor::~ImageProcessor()
{
  m_workerThread.quit();
  m_workerThread.wait();
}

void ImageProcessor::setClipBoard(const QString &data) {
  QClipboard *cb = QGuiApplication::clipboard();
  cb->setText(data);
}

void ImageProcessorWorker::sslErrors(const QList<QSslError> &sslErrors)
{
#if QT_CONFIG(ssl)
  for (const QSslError &error : sslErrors)
    fprintf(stderr, "SSL error: %s\n", qPrintable(error.errorString()));
#else
  Q_UNUSED(sslErrors);
#endif
}


void ImageProcessor::download(const QUrl &url)
{
  emit startDownload(url);
}

void ImageProcessor::downloadList(const QList<QUrl> &urls)
{
  for(const auto &url : urls) {
    emit startDownload(url);
  }
}

QString ImageProcessor::urlFileName(const QUrl &url)
{
  return url.fileName();
}

void ImageProcessor::ready(const QUrl &url, const QString &hash)
{
  emit imageReady(hash, url);
}

bool ImageProcessorWorker::isHttpRedirect(QNetworkReply *reply)
{
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  return statusCode == 301 || statusCode == 302 || statusCode == 303
      || statusCode == 305 || statusCode == 307 || statusCode == 308;
}

ImageProcessorWorker::ImageProcessorWorker(QObject *parent) : QObject(parent)
{

}

void ImageProcessorWorker::startDownload(const QUrl &url)
{
  if(manager == nullptr) {
    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)),
            SLOT(downloadFinished(QNetworkReply*)));
  }

  QNetworkRequest req(url);
  QNetworkReply *reply = manager->get(req);
#if QT_CONFIG(ssl)
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
          SLOT(sslErrors(QList<QSslError>)));
#endif
}

void ImageProcessorWorker::downloadFinished(QNetworkReply *reply)
{
  QUrl url = reply->url();
  if (reply->error()) {
    qWarning("Download of %s failed: %s",
             url.toEncoded().constData(),
             qPrintable(reply->errorString()));
  } else {
    if (isHttpRedirect(reply)) {
      qInfo("Request was redirected.");
    } else {
      QByteArray bytes = reply->readAll();
      QByteArray hashBytes = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
      QString hashString(hashBytes.toHex());

      ImageDao *tip = ImageDao::instance();
      tip->insert(hashString, bytes);

      emit ready(url, hashString);
    }
  }

  reply->deleteLater();
}
