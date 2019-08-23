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

ImageProcessor::ImageProcessor(QObject *parent) : QObject(parent)
{
  ImageFetcher *fetcher = new ImageFetcher();
  ImageDatabaseWriter *writer = new ImageDatabaseWriter();

  fetcher->moveToThread(&m_downloadThread);
  writer->moveToThread(&m_writeThread);

  connect(&m_downloadThread, &QThread::finished, fetcher, &QObject::deleteLater);
  connect(&m_writeThread, &QThread::finished, writer, &QObject::deleteLater);

  connect(this, &ImageProcessor::startDownload, fetcher, &ImageFetcher::startDownload);
  connect(fetcher, &ImageFetcher::downloadComplete, writer, &ImageDatabaseWriter::startWrite);
  connect(writer, &ImageDatabaseWriter::writeComplete, this, &ImageProcessor::imageReady);


  m_downloadThread.start();
  m_writeThread.start();
}

ImageProcessor::~ImageProcessor()
{
  m_downloadThread.quit();
  m_writeThread.quit();

  m_downloadThread.wait();
  m_writeThread.wait();

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
      qInfo("Request was redirected.");
    } else {
      QByteArray bytes = reply->readAll();
      /*
      QBuffer buffer(&bytes);
      QImageReader reader(&buffer);
      qInfo("Type: %s", reader.format().constData());
      QSize size = reader.size();
      qInfo("Size: %dx%d", size.width(), size.height());
      int targetSize = 512;
      float scalex = (float)targetSize / size.width();
      float scaley = (float)targetSize / size.height();

      QSize newSize;
      if(scalex > scaley) {
        newSize.setWidth(targetSize);
        newSize.setHeight(size.height() * scalex);
      } else {
        newSize.setWidth(size.width() * scaley);
        newSize.setHeight(targetSize);
      }

      reader.setScaledSize(newSize);

      int top = (newSize.height() - targetSize) / 2;
      int left = (newSize.width() - targetSize) / 2;

      QRect clip(left, top, targetSize, targetSize);
      reader.setScaledClipRect(clip);
      reader.setBackgroundColor(Qt::black);
      QImage image = reader.read();

      QByteArray thumbNail;
      QBuffer output(&thumbNail);
      output.open(QIODevice::WriteOnly);
      image.save(&output, "JPG");
      output.close();

      qInfo("Thumbnail: %d bytes", thumbNail.size());
      */

      emit downloadComplete(url, bytes);
    }
  }

  reply->deleteLater();
}

ImageDatabaseWriter::ImageDatabaseWriter(QObject *parent) : QObject(parent), timer(this)
{
  timer.setInterval(0);
  timer.setSingleShot(true);

  connect(&timer, &QTimer::timeout, this, &ImageDatabaseWriter::drainQueue);
}

void ImageDatabaseWriter::startWrite(const QUrl &url, const QByteArray &data)
{
  writeQueue.append({ url, data, {}});
  if(!timer.isActive())
    timer.start();
}

void ImageDatabaseWriter::drainQueue()
{
  ImageDao *dao = ImageDao::instance();

  SQLiteConnectionPool *pool = dao->connPool();
  SQLiteConnection *conn = pool->open();
  SQLitePreparedStatement ps;
  ps.init(conn->m_db, "INSERT OR IGNORE INTO store (hash, date, image) VALUES (?1, datetime(), ?2)");

  dao->lockWrite();
  conn->exec("BEGIN TRANSACTION", __FUNCTION__);

  for(ImageData &id : writeQueue) {
    QByteArray hashBytes = QCryptographicHash::hash(id.data, QCryptographicHash::Sha256);
    id.hash = hashBytes.toHex();
    ps.bind(1, id.hash);
    ps.bind(2, id.data);
    ps.step(__FUNCTION__);
    ps.reset();
  }

  ps.destroy();
  conn->exec("END TRANSACTION", __FUNCTION__);
  pool->close(conn);
  dao->unlockWrite();

  for(ImageData &id : writeQueue) {
    emit writeComplete(id.url, id.hash);
  }

  writeQueue.clear();
}
