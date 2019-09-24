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
  Q_PROPERTY(QStringList tags READ tags NOTIFY tagsChanged)
  Q_PROPERTY(QSize size READ size CONSTANT)
  Q_PROPERTY(bool deleted MEMBER m_deleted NOTIFY deletedChanged)
  Q_PROPERTY(QString format MEMBER m_format CONSTANT)
  Q_PROPERTY(qint64 fileSize MEMBER m_fileSize CONSTANT)
  Q_PROPERTY(QString overlayFormat MEMBER m_overlayFormat NOTIFY overlayFormatChanged)
  Q_PROPERTY(QString overlayString READ overlayString NOTIFY overlayStringChanged STORED false)
public:
  ImageRef(QObject *parent = nullptr);

  qint64 m_fileId = 0;
  bool m_selected = false;
  bool m_deleted = false;
  QSet<QString> m_tags;
  QSize m_size;
  qint64 m_phash = 0;
  QString m_format;
  qint64 m_fileSize = 0;
  QImage::Format m_pixelFormat = QImage::Format_Invalid;
  QString m_overlayFormat;

  QStringList tags() const;
  QSize size() const { return m_size; }
  QString overlayString() const;
signals:
  void selectedChanged();
  void tagsChanged();
  void deletedChanged();
  void overlayFormatChanged();
  void overlayStringChanged();
};

#endif // IMAGEREF_H
