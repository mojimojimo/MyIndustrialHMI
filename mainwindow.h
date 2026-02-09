#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
    //void updateUI(int type,double value);
    // 接收子线程的反馈
    void onPortStatusChanged(bool isOpen);
    void onDataReceived(int type, double value);

signals:
    void signalOpenSerial(QString portName,int baudRate);
    void signalCloseSerial();


private:
    Ui::MainWindow *ui;

};
#endif // MAINWINDOW_H
