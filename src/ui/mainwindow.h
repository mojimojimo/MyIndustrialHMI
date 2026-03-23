#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "devicemanager.h"
#include <QSettings>
#include <QLabel>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
public slots:
    // 接收子线程的反馈
    void onStatusChanged(DeviceState state);
    void onDataReceived(const DeviceData &data);//<-Device改！！！
    void writeLog(const QString& level, const QString& msg);

signals:
    //void signalSendData(char funcCode, const QByteArray &dataContent);//->Device

private:
    Ui::MainWindow *ui;

    void refreshPorts();
    void initChart();

    //void updateRealTimeUI(const DeviceData &data);

    void closeEvent(QCloseEvent *event);
    QTimer *refreshTimer = nullptr;
    QTimer *timer = nullptr;
    QLabel *lblCommStatus = nullptr;
    QLabel *lblGlobalAlarm = nullptr;

};
#endif // MAINWINDOW_H
