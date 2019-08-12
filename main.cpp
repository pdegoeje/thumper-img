#include "imageprocessor.h"
#include "thumperimageprovider.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

  QGuiApplication app(argc, argv);

  qmlRegisterType<ImageProcessor>("thumper", 1, 0, "ImageProcessor");
  qmlRegisterSingletonType<ThumperImageProvider>("thumper", 1, 0, "ThumperImageProvider",
    [](QQmlEngine *, QJSEngine *) { return (QObject *)ThumperImageProvider::instance(); });

  QQmlApplicationEngine engine;
  engine.addImageProvider(QLatin1String("thumper"), ThumperImageProvider::instance());

  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty())
    return -1;

  return app.exec();
}
