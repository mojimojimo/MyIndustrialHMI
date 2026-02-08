#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort>

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

    //槽函数
    void onReadyRead();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;//串口对象

    QByteArray m_buffer;//全局接收缓冲区
};
#endif // MAINWINDOW_H
