#ifndef THUMPER_H
#define THUMPER_H

#include <QObject>
#include <QDir>

class Thumper : public QObject
{
  Q_OBJECT

  Q_PROPERTY(QString databaseFilename READ databaseFilename CONSTANT)
public:
  QString m_databaseFilename = QStringLiteral("default.imgdb");

  explicit Thumper(QObject *parent = nullptr);

  QString databaseFilename() const { return m_databaseFilename; }
  Q_INVOKABLE QString databaseAbsolutePath() const;
  Q_INVOKABLE QString databaseRelativePath(const QString &path) const;
signals:

public slots:
};

#endif // THUMPER_H
