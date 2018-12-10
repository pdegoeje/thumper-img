#include "imageprocessor.h"

#include <QFile>
#include <QClipboard>
#include <QGuiApplication>
#include <QNetworkReply>

ImageProcessor::ImageProcessor(QObject *parent) : QObject(parent)
{
  connect(&manager, SIGNAL(finished(QNetworkReply*)),
          SLOT(downloadFinished(QNetworkReply*)));
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

bool ImageProcessor::saveToDisk(const QString &filename, QIODevice *data)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Could not open %s for writing: %s",
                qPrintable(filename),
                qPrintable(file.errorString()));
        return false;
    }

    file.write(data->readAll());
    file.close();

    return true;
}

QString ImageProcessor::saveFileName(const QUrl &url)
{
    QString path = url.path();
    QString basename = url.fileName();

    if (basename.isEmpty())
        basename = "download";

    if (QFile::exists(basename)) {
        // already exists, don't overwrite
        int i = 0;
        basename += '.';
        while (QFile::exists(basename + QString::number(i)))
            ++i;

        basename += QString::number(i);
    }

    return basename;
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
          QString filename = saveFileName(url);
          if (saveToDisk(filename, reply)) {
              qInfo("Download of %s succeeded (saved to %s)",
                     url.toEncoded().constData(), qPrintable(filename));
          }
      }
  }

  //currentDownloads.removeAll(reply);
  reply->deleteLater();
}

bool ImageProcessor::isHttpRedirect(QNetworkReply *reply)
{
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return statusCode == 301 || statusCode == 302 || statusCode == 303
           || statusCode == 305 || statusCode == 307 || statusCode == 308;
}
