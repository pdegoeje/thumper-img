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
  bool saveToDisk(const QString &filename, QIODevice *data);
  QString saveFileName(const QUrl &url);
public:
  explicit ImageProcessor(QObject *parent = nullptr);

  Q_INVOKABLE int nextId(const QString &prefix);
  Q_INVOKABLE void download(const QUrl &url);
signals:

public slots:

private slots:
  void sslErrors(const QList<QSslError> &sslErrors);
  void downloadFinished(QNetworkReply *reply);
};

#endif // IMAGEPROCESSOR_H
