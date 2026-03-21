#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qRegisterMetaType<DeviceData>("DeviceData");
    qRegisterMetaType<ConfigData>("ConfigData");
    qRegisterMetaType<QList<HistoryData>>("QList<HistoryData>");
    qRegisterMetaType<Frame>("Frame");

    MainWindow w;
    w.show();
    return a.exec();
}
