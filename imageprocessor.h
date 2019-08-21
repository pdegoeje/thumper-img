#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QTimer>
#include <QThread>

struct ImageBuffer {
  QUrl url;
  QString hash;
  QByteArray data;
};

class ImageProcessorWorker : public QObject {
  Q_OBJECT

  QNetworkAccessManager *manager = nullptr;
  bool isHttpRedirect(QNetworkReply *reply);
public:
  ImageProcessorWorker(QObject *parent = nullptr);
public slots:
  void startDownload(const QUrl &url);
private slots:
  void downloadFinished(QNetworkReply *reply);
  void sslErrors(const QList<QSslError> &sslErrors);
signals:
  void ready(const QUrl &url, const QByteArray &data, const QString &hash);
};

class ImageProcessor : public QObject
{
  Q_OBJECT

  QThread m_workerThread;
public:
  explicit ImageProcessor(QObject *parent = nullptr);
  virtual ~ImageProcessor();

  Q_INVOKABLE void setClipBoard(const QString &data);
  Q_INVOKABLE void download(const QUrl &url);
  Q_INVOKABLE QString urlFileName(const QUrl &url);
signals:
  void imageReady(const QString &hash, const QUrl &url);
  void startDownload(const QUrl &url);
public slots:
  void ready(const QUrl &url, const QByteArray &data, const QString &hash);
};

#endif // IMAGEPROCESSOR_H
