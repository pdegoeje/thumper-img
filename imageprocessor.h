#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QObject>
#include <QUrl>

class ImageProcessor : public QObject
{
  Q_OBJECT
public:
  explicit ImageProcessor(QObject *parent = nullptr);

  Q_INVOKABLE int nextId(const QString &prefix);
signals:

public slots:
};

#endif // IMAGEPROCESSOR_H
