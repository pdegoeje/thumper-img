#include "imageprocessor.h"
#include "thumperimageprovider.h"
#include "imagedao.h"
#include "sqlite3.h"
#include "taglist.h"
#include "fileutils.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QFile>

static void (*previousMessageHandler)(QtMsgType, const QMessageLogContext &, const QString &);
static QFile *logFile;

static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
  if(logFile == nullptr) {
    logFile = new QFile("flowsuite.log");
    if(!logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      fprintf(stderr, "Failed to open logfile\n");
    } else {
      fprintf(stderr, "Log file: %s\n", qUtf8Printable(logFile->fileName()));
    }
  }

  QString output = QString::asprintf("%s %s (%s:%u, %s)\n",
                                     qUtf8Printable(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddThh:mm:ss.zzz"))),
                                     qUtf8Printable(msg), context.file, context.line, context.function);
  logFile->write(output.toUtf8());
  logFile->flush();

  if(previousMessageHandler != nullptr) {
    previousMessageHandler(type, context, msg);
  }
}

int main(int argc, char *argv[])
{
  previousMessageHandler = qInstallMessageHandler(&messageHandler);

  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

  QGuiApplication app(argc, argv);

  qmlRegisterType<ImageRef>("thumper", 1, 0, "ImageRef");
  qmlRegisterType<ImageProcessor>("thumper", 1, 0, "ImageProcessor");
  qmlRegisterSingletonType<ImageDao>("thumper", 1, 0, "ImageDao",
    [](QQmlEngine *, QJSEngine *) { return (QObject *)ImageDao::instance(); });

  qmlRegisterType<QmlTaskListModel>("thumper", 1, 0, "QmlTaskListModel");
  qmlRegisterType<FileUtils>("thumper", 1, 0, "FileUtils");
  //qmlRegisterType<Tag>("thumper", 1, 0, "TagModel");
  //qmlRegisterType<TagListModel>("thumper", 1, 0, "TagModelList");

  QQmlApplicationEngine engine;
  engine.addImageProvider(QLatin1String("thumper"), new ThumperAsyncImageProvider());

  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty())
    return -1;

  return app.exec();
}
