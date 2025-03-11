#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QThread>
#include <QTimer>

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

class ImageProcessor : public QObject
{
  Q_OBJECT

  QThread m_downloadThread;
public:
  explicit ImageProcessor(QObject *parent = nullptr);
  virtual ~ImageProcessor();

  Q_INVOKABLE void setClipBoard(const QString &data);
  Q_INVOKABLE void download(const QUrl &url);
  Q_INVOKABLE void downloadText(const QString &str);
  Q_INVOKABLE void downloadList(const QList<QUrl> &urls);
  Q_INVOKABLE QString urlFileName(const QUrl &url);
  Q_INVOKABLE bool isUrl(const QString &text);
  Q_INVOKABLE QList<QUrl> parseTextUriList(const QString &text);
signals:
  void imageReady(const QUrl &url, quint64 fileId);
  void startDownload(const QUrl &url);
};

#endif // IMAGEPROCESSOR_H
