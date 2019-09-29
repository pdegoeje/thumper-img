#ifndef IMAGEREF_H
#define IMAGEREF_H

#include <QObject>
#include <QSize>
#include <QSet>
#include <QDebug>
#include <QImage>

class ImageRef : public QObject {
  Q_OBJECT

  Q_PROPERTY(qint64 fileId MEMBER m_fileId CONSTANT)
  Q_PROPERTY(bool selected MEMBER m_selected NOTIFY selectedChanged)
  Q_PROPERTY(bool deleted MEMBER m_deleted NOTIFY deletedChanged)
  Q_PROPERTY(QString overlayFormat MEMBER m_overlayFormat NOTIFY overlayFormatChanged)
  Q_PROPERTY(QString overlayString READ overlayString NOTIFY overlayStringChanged STORED false)

  Q_PROPERTY(QSize size READ size CONSTANT)
  Q_PROPERTY(QString url MEMBER m_url CONSTANT)

  Q_PROPERTY(QStringList tags READ tags NOTIFY tagsChanged)
  Q_PROPERTY(qint64 fileSize MEMBER m_fileSize CONSTANT)
  Q_PROPERTY(QString format MEMBER m_format CONSTANT)
public:
  ImageRef(QObject *parent = nullptr);

  QSet<QString> m_tags;
  QSize m_size;
  QString m_format;
  QString m_url;
  QString m_overlayFormat;
  qint64 m_fileId = 0;
  qint64 m_phash = 0;
  qint64 m_fileSize = 0;
  QImage::Format m_pixelFormat = QImage::Format_Invalid;

  bool m_selected = false;
  bool m_deleted = false;

  QStringList tags() const;
  QSize size() const { return m_size; }
  QString overlayString() const;

  void updateImageData(const QString &newFormat, qint64 newFileSize, QImage::Format newPixelFormat);
signals:
  void selectedChanged();
  void tagsChanged();
  void deletedChanged();
  void overlayFormatChanged();
  void overlayStringChanged();
};

#endif // IMAGEREF_H
