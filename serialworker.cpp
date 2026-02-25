#include "serialworker.h"
#include<QDebug>

SerialWorker::SerialWorker(QObject *parent)
    : QObject{parent}
{
    serial = new QSerialPort(this);

    //串口接收缓冲区有数据
    connect(serial,&QSerialPort::readyRead,this,&SerialWorker::onReadyRead);
    //连接底层错误信号
    connect(serial,&QSerialPort::errorOccurred,this,&SerialWorker::handleError);
}

SerialWorker::~SerialWorker(){
    if(serial->isOpen()){
        serial->close();
    }
}

void SerialWorker::onReadyRead(){
    QByteArray data = serial->readAll();
//.toHex(' ').toUpper()
    emit rawDataReceived(data);
}

void SerialWorker::openSerialPort(QString portName,int baudRate){

    if (serial->isOpen()) serial->close();

    //配置串口参数
    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    //8N1
    // serial->setDataBits(QSerialPort::Data8);
    // serial->setParity(QSerialPort::NoParity);
    // serial->setStopBits(QSerialPort::OneStop);

    //打开串口
    if(serial->open(QIODevice::ReadWrite)){
        qDebug() << "串口打开成功：COM1";
        emit portStatusChanged(true);
    } else {
        emit errorOccuerred(serial->errorString());
        emit portStatusChanged(false);
        qDebug() << "串口打开失败："<<serial->errorString();

    }
}

void SerialWorker::closeSerialPort(){
    serial->close();
    emit portStatusChanged(false);
}

void SerialWorker::sendData(const QByteArray &data){
    //发数据 qint64
    //QByteArray sendData = QByteArray::fromHex("AA55020119FF");//AA 55 02 01 19 3F
    if (serial && serial->isOpen()) {
        serial->write(data);
    }
}


void SerialWorker::handleError(QSerialPort::SerialPortError error){
    //虚拟串口无 “硬件检测”
    if(error == QSerialPort::NoError) return;//没错误
    QString errorStr;
    bool needClose = false;

    switch(error){
    case QSerialPort::ResourceError:
        errorStr = "设备连接中断：检查是否拔线或掉电！";
        needClose = true;
        break;
    case QSerialPort::PermissionError:
        errorStr = "权限不足（设备被占用）！";
        needClose = true;
        break;
    case QSerialPort::DeviceNotFoundError:
        errorStr = "找不到指定设备！";
        needClose = true;
        break;
    default:
        return;
    }

    if(needClose){
        if(serial->isOpen()){
            serial->close();
        }
        emit portStatusChanged(false);
        emit errorOccuerred(errorStr);
    }
}
