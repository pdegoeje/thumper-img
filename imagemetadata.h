#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QObject>
#include <QRunnable>
#include <QImage>

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

  uint64_t perceptualHash(const QImage &image);
  uint64_t blockHash(const QImage &image);
  uint64_t differenceHash(const QImage &image);

  QImage autoCrop(const QImage &image, int threshold);

  void run() override;
};

#endif // IMAGEMETADATA_H
