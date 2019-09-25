#include "imageprocessor.h"
#include "imagedao.h"

#include <QFile>
#include <QClipboard>
#include <QGuiApplication>
#include <QNetworkReply>
#include <QPixmap>
#include <QImageReader>
#include <QDir>
#include <QBuffer>
#include <QColor>
#include <QTextStream>

ImageProcessor::ImageProcessor(QObject *parent) : QObject(parent)
{
  ImageFetcher *fetcher = new ImageFetcher();
  fetcher->moveToThread(&m_downloadThread);

  connect(&m_downloadThread, &QThread::finished, fetcher, &QObject::deleteLater);

  connect(this, &ImageProcessor::startDownload, fetcher, &ImageFetcher::startDownload);
  connect(fetcher, &ImageFetcher::downloadComplete, ImageDao::instance(), &ImageDao::deferredWriteImage);
  connect(ImageDao::instance(), &ImageDao::writeComplete, this, &ImageProcessor::imageReady);

  m_downloadThread.start();
}

ImageProcessor::~ImageProcessor()
{
  m_downloadThread.quit();
  m_downloadThread.wait();

  qInfo(__FUNCTION__);
}

void ImageProcessor::setClipBoard(const QString &data) {
  QClipboard *cb = QGuiApplication::clipboard();
  cb->setText(data);
}

void ImageFetcher::sslErrors(const QList<QSslError> &sslErrors)
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

void ImageProcessor::downloadText(const QString &str) {
  qDebug() << "Text url" << str;
  //download(QUrl(str));
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

bool ImageProcessor::isUrl(const QString &text)
{
  QUrl url(text, QUrl::TolerantMode);
  return url.isValid();
}

QList<QUrl> ImageProcessor::parseTextUriList(const QString &text)
{
  QList<QUrl> result;
  QTextStream str((QString *)&text, QIODevice::ReadOnly);

  QString line;
  while(!(line = str.readLine()).isNull()) {
    QUrl url(line);
    if(url.isValid())
      result.append(url);
  }

  return result;
}

bool ImageFetcher::isHttpRedirect(QNetworkReply *reply)
{
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  return statusCode == 301 || statusCode == 302 || statusCode == 303
      || statusCode == 305 || statusCode == 307 || statusCode == 308;
}

ImageFetcher::ImageFetcher(QObject *parent) : QObject(parent), manager(this)
{
  connect(&manager, SIGNAL(finished(QNetworkReply*)),
          SLOT(downloadFinished(QNetworkReply*)));
}

void ImageFetcher::startDownload(const QUrl &url)
{
  QNetworkRequest req(url);
  QNetworkReply *reply = manager.get(req);

#if QT_CONFIG(ssl)
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
          SLOT(sslErrors(QList<QSslError>)));
#endif

}

void ImageFetcher::downloadFinished(QNetworkReply *reply)
{
  QUrl url = reply->url();
  if (reply->error()) {
    qWarning("Download of %s failed: %s",
             url.toEncoded().constData(),
             qPrintable(reply->errorString()));
  } else {
    if (isHttpRedirect(reply)) {
      QUrl newLocation = reply->header(QNetworkRequest::LocationHeader).toUrl();
      qDebug() << "Request was redirected. Location:" << newLocation;
      emit startDownload(newLocation);
    } else {
      QByteArray bytes = reply->readAll();
      emit downloadComplete(url, bytes);
    }
  }

  reply->deleteLater();
}
