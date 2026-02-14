#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include "serialworker.h"
#include <QSettings>
#include <QElapsedTimer>

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
    void onPortStatusChanged(bool isOpen);
    void onDataReceived(int type, double value);

signals:
    void signalOpenSerial(QString portName,int baudRate);
    void signalCloseSerial();
    void signalSendData(QByteArray data);


private:
    Ui::MainWindow *ui;
    QThread *thread = nullptr;
    QTimer *timer = nullptr;
    QTimer *timeoutTimer = nullptr;
    QElapsedTimer responseTimer;
    void refreshPorts();
    void initChart();
    QByteArray buildPacket(char funcCode, const QByteArray &dataContent);//封包
    void writeLog(const QString &text,bool isSend);//?
    void closeEvent(QCloseEvent *event);

};
#endif // MAINWINDOW_H
