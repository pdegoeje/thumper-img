#include "imageprocessor.h"
#include "thumperimageprovider.h"
#include "imagedao.h"
#include "sqlite3.h"
#include "taglist.h"
#include "fileutils.h"
#include "thumper.h"

#include <QApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QFile>
#include <QQmlContext>
#include <QStandardPaths>

static void (*msgPrevHandler)(QtMsgType, const QMessageLogContext &, const QString &);
static QFile msgLogFile;
static QBasicMutex msgMutex;

static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
  {
    QString output = QString::asprintf("%s %s\n",
                                       qUtf8Printable(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddThh:mm:ss.zzz"))),
                                       qUtf8Printable(msg));

    QMutexLocker lock(&msgMutex);
    msgLogFile.write(output.toUtf8());
    msgLogFile.flush();
  }

  if(msgPrevHandler != nullptr) {
    msgPrevHandler(type, context, msg);
  }
}

static void installFileLogging(const QString &logfile) {
  msgLogFile.setFileName(logfile);
  if(!msgLogFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    fprintf(stderr, "Failed to open logfile: %s\n", qUtf8Printable(logfile));
  } else {
    fprintf(stderr, "Log file: %s\n", qUtf8Printable(logfile));
  }
  msgPrevHandler = qInstallMessageHandler(&messageHandler);
}

int main(int argc, char *argv[])
{
  Thumper thumper;

  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication app(argc, argv);

  QDir docDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  QString dbname = docDir.filePath(QStringLiteral("thumper.imgdb"));

  auto args = app.arguments();
  if(args.length() >= 2) {
    dbname = args.at(1);
  }

  QFileInfo file(dbname);
  file.makeAbsolute();
  thumper.m_databaseFilename = file.filePath();

  installFileLogging(thumper.databaseFilename() + ".debug.log");
  ImageDao::setDatabaseFilename(thumper.databaseFilename());

  qmlRegisterUncreatableType<Thumper>("thumper", 1, 0, "Thumper", QString("Must be created before QML engine starts"));
  qmlRegisterType<ImageRef>("thumper", 1, 0, "ImageRef");
  qmlRegisterType<ImageProcessor>("thumper", 1, 0, "ImageProcessor");
  qmlRegisterSingletonType<ImageDao>("thumper", 1, 0, "ImageDao",
    [](QQmlEngine *, QJSEngine *) { return (QObject *)ImageDao::instance(); });

  qmlRegisterType<QmlTaskListModel>("thumper", 1, 0, "QmlTaskListModel");
  qmlRegisterType<FileUtils>("thumper", 1, 0, "FileUtils");
  qmlRegisterType<ImageDaoProgress>("thumper", 1, 0, "ImageDaoProgress");
  //qmlRegisterType<Tag>("thumper", 1, 0, "TagModel");
  //qmlRegisterType<TagListModel>("thumper", 1, 0, "TagModelList");

  QQmlApplicationEngine engine;
  engine.addImageProvider(QLatin1String("thumper"), new ThumperAsyncImageProvider());
  engine.rootContext()->setContextProperty("thumper", &thumper);
  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty())
    return -1;

  return app.exec();
}
