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
  connect(&m_timer, &QTimer::timeout, this, &ImageProcessor::drainQueue);
  m_timer.setInterval(100);
  m_timer.setSingleShot(true);

  ImageProcessorWorker *worker = new ImageProcessorWorker();
  worker->moveToThread(&m_workerThread);

  connect(&m_workerThread, &QThread::finished, worker, &QObject::deleteLater);
  connect(this, &ImageProcessor::startDownload, worker, &ImageProcessorWorker::startDownload);
  connect(worker, &ImageProcessorWorker::ready, this, &ImageProcessor::ready);

  m_workerThread.start();

  qInfo("construct");
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


void ImageProcessor::download(const QUrl &url) {
  qInfo("ImageProcessor::download %d", QThread::currentThreadId());
  emit startDownload(url);
}

QString ImageProcessor::urlFileName(const QUrl &url)
{
  return url.fileName();
}

void ImageProcessor::ready(const QUrl &url, const QByteArray &data, const QString &hash)
{
  m_buffers.append({url, hash, data});
  if(!m_timer.isActive()) {
    m_timer.start();
  }
}

void ImageProcessor::drainQueue()
{
  qInfo("Drain queue: %d", m_buffers.length());
  ImageDao *tip = ImageDao::instance();
  tip->transactionStart();
  while(!m_buffers.isEmpty()) {
    ImageBuffer ib = m_buffers.front();
    tip->insert(ib.hash, ib.data);
    m_buffers.pop_front();

    emit imageReady(ib.hash, ib.url);
  }
  tip->transactionEnd();
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
  qInfo("ImageProcessorWorker::startDownload %d", QThread::currentThreadId());

  if(manager == nullptr) {
    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)),
            SLOT(downloadFinished(QNetworkReply*)));
  }

  QNetworkRequest req(url);
  QNetworkReply *reply = manager->get(req);

  qInfo("ImageProcessorWorker::startDownload END");

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
      QByteArray hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
      QString key(hash.toHex());

      qInfo("Download of %s succeeded (%s/%d)", url.toEncoded().constData(), qUtf8Printable(key), bytes.length());

      emit ready(url, bytes, key);
    }
  }

  reply->deleteLater();
}
