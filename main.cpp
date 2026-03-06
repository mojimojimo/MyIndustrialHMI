#include "mainwindow.h"
#include "databasemanager.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    //qRegisterMetaType<Frame>("Frame");
    QApplication a(argc, argv);

    //初始化数据库
    if (!DatabaseManager::instance().init()) {
        return -1;
    }
    MainWindow w;
    w.show();
    return a.exec();
}
