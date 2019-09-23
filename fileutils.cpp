#include "fileutils.h"

#include <QQuickWindow>
#include <QTextStream>
#include <QFile>
#include <QDebug>
#include <QFileDialog>

FileUtils::FileUtils(QObject *parent) : QObject(parent)
{
  connect(&m_fileDialog, &QFileDialog::fileSelected, this, &FileUtils::imageDatabaseSelected);
  m_fileDialog.setNameFilter(QString("Image Database (*.imgdb)"));
  m_fileDialog.setWindowTitle(QStringLiteral("Open Image Database"));
  m_fileDialog.setFileMode(QFileDialog::AnyFile);
  m_fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
  m_fileDialog.setOption(QFileDialog::DontConfirmOverwrite);
  m_fileDialog.setDefaultSuffix(QStringLiteral("imgdb"));
}

void FileUtils::save(const QString &path, const QString &data)
{
  QFile file(path);
  if(!file.open(QFile::WriteOnly | QFile::Truncate)) {
    qDebug() << __FUNCTION__ << "Failed to open file" << path;
    return;
  }

  file.write(data.toUtf8());
  file.close();
}

QString FileUtils::load(const QString &path)
{
  QFile file(path);
  if(!file.open(QFile::ReadOnly)) {
    qDebug() << __FUNCTION__ << "Failed to open file" << path;
    return {};
  }

  return QString::fromUtf8(file.readAll());
  file.close();
}

void FileUtils::openImageDatabase()
{
  m_fileDialog.open();
}
