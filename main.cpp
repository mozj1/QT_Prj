#include "app/appcontroller.h"
#include "mainwindow.h"

#include <QCoreApplication>
#include <QApplication>
#include <QLocale>
#include <QQuickStyle>
#include <QTranslator>

/**
 * @brief 应用程序入口，初始化 QApplication、翻译资源和 QMainWindow 主界面。
 * @author mozhengjie
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return int Qt 应用程序退出码。
 */
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = QStringLiteral("Modbus_App_") + QLocale(locale).name();
        if (translator.load(QStringLiteral(":/i18n/") + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    AppController appController;
    MainWindow mainWindow(&appController);
    mainWindow.show();
    return QCoreApplication::exec();
}
