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
  bool saveToDisk(QIODevice *data);
public:
  explicit ImageProcessor(QObject *parent = nullptr);

  Q_INVOKABLE int nextId(const QString &prefix);
  Q_INVOKABLE void download(const QUrl &url);
  Q_INVOKABLE void loadExisting();
signals:
  void imageReady(const QString &fileId);
public slots:

private slots:
  void sslErrors(const QList<QSslError> &sslErrors);
  void downloadFinished(QNetworkReply *reply);
};

#endif // IMAGEPROCESSOR_H
