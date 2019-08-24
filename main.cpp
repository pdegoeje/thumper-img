#include "imageprocessor.h"
#include "thumperimageprovider.h"
#include "imagedao.h"
#include "sqlite3.h"
#include "taglist.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

  QGuiApplication app(argc, argv);

  qmlRegisterType<ImageRef>("thumper", 1, 0, "ImageRef");
  qmlRegisterType<ImageProcessor>("thumper", 1, 0, "ImageProcessor");
  qmlRegisterSingletonType<ImageDao>("thumper", 1, 0, "ImageDao",
    [](QQmlEngine *, QJSEngine *) { return (QObject *)ImageDao::instance(); });

  qmlRegisterType<QmlTaskListModel>("thumper", 1, 0, "QmlTaskListModel");
  qmlRegisterType<Tag>("thumper", 1, 0, "TagModel");
  qmlRegisterType<TagListModel>("thumper", 1, 0, "TagModelList");

  QQmlApplicationEngine engine;
  engine.addImageProvider(QLatin1String("thumper"), new ThumperAsyncImageProvider());

  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty())
    return -1;

  return app.exec();
}
