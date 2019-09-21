#ifndef IMAGEREF_H
#define IMAGEREF_H

#include <QObject>
#include <QSize>
#include <QSet>
#include <QDebug>

class ImageRef : public QObject {
  Q_OBJECT

  Q_PROPERTY(qint64 fileId MEMBER m_fileId CONSTANT)
  Q_PROPERTY(bool selected MEMBER m_selected NOTIFY selectedChanged)
  Q_PROPERTY(QStringList tags READ tags NOTIFY tagsChanged)
  Q_PROPERTY(QSize size READ size CONSTANT)
  Q_PROPERTY(bool deleted MEMBER m_deleted NOTIFY deletedChanged)

public:
  ImageRef(QObject *parent = nullptr) : QObject(parent) { }

  qint64 m_fileId = 0;
  bool m_selected = false;
  bool m_deleted = false;
  QSet<QString> m_tags;
  QSize m_size;
  qint64 m_phash = 0;

  QStringList tags();
  QSize size() const { return m_size; }
signals:
  void selectedChanged();
  void tagsChanged();
  void deletedChanged();
};

#endif // IMAGEREF_H
