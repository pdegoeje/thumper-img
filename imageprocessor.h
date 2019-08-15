#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>

class ImageProcessor : public QObject
{
  Q_OBJECT

  QNetworkAccessManager manager;
  bool isHttpRedirect(QNetworkReply *reply);
  bool saveToDisk(QNetworkReply *data);
public:
  explicit ImageProcessor(QObject *parent = nullptr);

  Q_INVOKABLE void setClipBoard(const QString &data);
  Q_INVOKABLE void download(const QUrl &url);
  Q_INVOKABLE QString urlFileName(const QUrl &url);
signals:
  void imageReady(const QString &fileId, const QUrl &url);
public slots:

private slots:
  void sslErrors(const QList<QSslError> &sslErrors);
  void downloadFinished(QNetworkReply *reply);
};

#endif // IMAGEPROCESSOR_H
