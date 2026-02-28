#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include "serialworker.h"
#include "protocolparser.h"
#include "tcpworker.h"
#include "devicemanager.h"
#include <QSettings>

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
    void onStatusChanged(bool isOpen);
    void onDataReceived(int type, double value);//<-Device
    void writeLog(const QString &text,bool isSend);

signals:
    void signalOpen(QString portName,int baudRate);//->Serial
    void signalClose();//->Serial
    void signalSendData(char funcCode, const QByteArray &dataContent);//->Device
    void signalDeviceStart(bool toStart);

private:
    Ui::MainWindow *ui;
    QThread *thread = nullptr;

    void refreshPorts();
    void initChart();
    void closeEvent(QCloseEvent *event);

};
#endif // MAINWINDOW_H
