#include <QApplication>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    // === 高 DPI 支持(Windows 模糊修复) ===
    // Qt 6 默认启用 AA_EnableHighDpiScaling,这里显式设置确保生效
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QApplication app(argc, argv);

    // === 加载全局样式表 ===
    QFile qssFile;
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/../resources/style.qss",   // build/Release -> resources/
        QCoreApplication::applicationDirPath() + "/resources/style.qss",       // 与 exe 同目录
        ":/resources/style.qss"                                                 // Qt 资源系统(预留)
    };
    for (const auto& path : searchPaths) {
        qssFile.setFileName(path);
        if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            app.setStyleSheet(qssFile.readAll());
            qssFile.close();
            break;
        }
    }

    MainWindow window;
    window.resize(800, 600);
    window.show();

    return app.exec();
}
