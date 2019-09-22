#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QObject>
#include <QRunnable>
#include <QImage>
#include <QByteArray>

QList<QObject *> findAllDuplicates(const QList<QObject *> &irefs, int maxDistance);
uint64_t perceptualHash(const QImage &image);
uint64_t blockHash(const QImage &image);
uint64_t differenceHash(const QImage &image);
QImage autoCrop(const QImage &image, int threshold);
bool updateImageMetaData(struct SQLiteConnection *m_conn, const QByteArray &data, quint64 id);

class ImageProcessStatus : public QObject {
  Q_OBJECT
public:
signals:
  void update(const QString &status, qreal fractionComplete);
  void complete();
};

class FixImageMetaDataTask : public QRunnable {
  ImageProcessStatus *status;

public:
  FixImageMetaDataTask(ImageProcessStatus *status) : status(status) { }
  void run() override;
};

#endif // IMAGEMETADATA_H
