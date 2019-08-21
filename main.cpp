#include "imageprocessor.h"
#include "thumperimageprovider.h"
#include "imagedao.h"
#include "sqlite3.h"

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

  QQmlApplicationEngine engine;
  engine.addImageProvider(QLatin1String("thumper"), new ThumperAsyncImageProvider());

  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty())
    return -1;

  return app.exec();
}
