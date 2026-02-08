#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    serial = new QSerialPort(this);
    serial->setPortName("COM1");
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setParity(QSerialPort::NoParity);

    //打开串口
    if(serial->open(QIODevice::ReadWrite)){//openmode?
        qDebug() << "串口打开成功：COM1";
    } else {
        qDebug() << "串口打开失败："<<serial->errorString();
    }

    //note:串口接收缓冲区有数据,?
    connect(serial,&QSerialPort::readyRead,this,&MainWindow::onReadyRead);

    //发数据 qint64
    QByteArray sendData = QByteArray::fromHex("AA55020119FF");//AA 55 02 01 19 3F
    serial->write(sendData);
}

MainWindow::~MainWindow()
{
    if(serial->isOpen()){
        serial->close();
    }
    delete ui;
}

void MainWindow::onReadyRead(){
    // note:QByteArray data = serial->readAll();
    // note:qDebug()<<"数据："<<data<<" "<<data.toHex(' ');

    // 1. 读取所有缓冲区里的数据
    QByteArray data = serial->readAll();

    // 2. 打印原始 Hex 数据
    qDebug() << "收到数据(Hex):" << data.toHex(' ');

    // 3. 简单解析验证
    if (!data.isEmpty()) {
        // 只发一包完整的数据
        if (data.at(0) == (char)0xAA && data.at(1) == (char)0x55) {
            qDebug() << "检测到帧头！";

            // 提取功能码
            if (data.size() > 2) {
                unsigned char funcCode = (unsigned char)data.at(2);
                if (funcCode == 0x01) {
                    qDebug() << "这是温度数据";
                }
            }
        }
    }
}
