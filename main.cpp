#include "widget.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

/**
 * @brief 应用程序入口，初始化 Qt 环境、加载翻译并显示主窗口。
 * @author mozhengjie
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return int Qt 应用程序退出码。
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyleSheet(QStringLiteral(
        "QMessageBox { background: #FFFFFF; color: #000000; }"
        "QMessageBox QWidget { background: #FFFFFF; color: #000000; }"
        "QMessageBox QLabel { background: #FFFFFF; color: #000000; }"
        "QMessageBox QPushButton { background: #FFFFFF; border: 1px solid #A8A8A8; padding: 5px 16px; color: #000000; }"
        "QMessageBox QPushButton:hover { background: #F3F3F3; }"
        "QMessageBox QPushButton:pressed { background: #E8E8E8; }"));

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "Modbus_App_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    Widget w;
    w.show();
    return QCoreApplication::exec();
}
