#ifndef THUMPER_H
#define THUMPER_H

#include <QObject>
#include <QDir>

#include <functional>

class Thumper : public QObject
{
  Q_OBJECT

  Q_PROPERTY(QString databaseFilename READ databaseFilename CONSTANT)
public:
  QString m_databaseFilename = QStringLiteral("default.imgdb");

  explicit Thumper(QObject *parent = nullptr);

  QString databaseFilename() const { return m_databaseFilename; }
  Q_INVOKABLE QString databasePath() const;
  Q_INVOKABLE QString resolveRelativePath(const QString &path) const;

  static QString textTemplate(const QString &blueprint, std::function<QString (const QStringRef &tag)> tagFunc);
signals:

public slots:
};

#endif // THUMPER_H
