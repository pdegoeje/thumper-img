#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QThread>

class ImageFetcher : public QObject {
  Q_OBJECT

  QNetworkAccessManager manager;
  bool isHttpRedirect(QNetworkReply *reply);
public:
  ImageFetcher(QObject *parent = nullptr);
public slots:
  void startDownload(const QUrl &url);
private slots:
  void downloadFinished(QNetworkReply *reply);
  void sslErrors(const QList<QSslError> &sslErrors);
signals:
  void downloadComplete(const QUrl &url, const QByteArray &data);
};

class ImageDatabaseWriter : public QObject {
  Q_OBJECT
public:
  ImageDatabaseWriter(QObject *parent = nullptr);
public slots:
  void startWrite(const QUrl &url, const QByteArray &data);
signals:
  void writeComplete(const QUrl &url, const QString &hash);
};

class ImageProcessor : public QObject
{
  Q_OBJECT

  QThread m_downloadThread;
  QThread m_writeThread;
public:
  explicit ImageProcessor(QObject *parent = nullptr);
  virtual ~ImageProcessor();

  Q_INVOKABLE void setClipBoard(const QString &data);
  Q_INVOKABLE void download(const QUrl &url);
  Q_INVOKABLE void downloadList(const QList<QUrl> &urls);
  Q_INVOKABLE QString urlFileName(const QUrl &url);
signals:
  void imageReady(const QUrl &url, const QString &hash);
  void startDownload(const QUrl &url);
//public slots:
//  void ready(const QUrl &url, const QString &hash);
};

#endif // IMAGEPROCESSOR_H
