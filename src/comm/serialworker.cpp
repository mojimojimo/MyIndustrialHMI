#include "serialworker.h"
#include <QDebug>

SerialWorker::SerialWorker(CommWorker *parent)
    : CommWorker{parent}
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
    emit rawDataReceived(data);
}

void SerialWorker::open(QString target, int portOrBaud){
    if (serial->isOpen()) serial->close();

    //配置串口参数
    serial->setPortName(target);
    serial->setBaudRate(portOrBaud);
    // 预留: 如需与设备协议严格对齐，可显式配置8N1。
    // serial->setDataBits(QSerialPort::Data8);
    // serial->setParity(QSerialPort::NoParity);
    // serial->setStopBits(QSerialPort::OneStop);

    //打开串口
    if(serial->open(QIODevice::ReadWrite)){
        emit logComm("INFO", "串口打开成功");
        emit StatusChanged(true);
    } else {
        // 预留: 可恢复errorOccurred信号链路以区分日志与业务错误弹窗。
        // emit errorOccurred(serial->errorString());
        emit logComm("ERROR", QString("串口打开失败：%1").arg(serial->errorString()));
        emit StatusChanged(false);
    }
}

void SerialWorker::close(){

    serial->close();
    emit StatusChanged(false);
    emit logComm("INFO", "串口已关闭");
}

void SerialWorker::sendData(const QByteArray &data){
    if (serial && serial->isOpen()) {
        serial->write(data);
    }
}


void SerialWorker::handleError(QSerialPort::SerialPortError error){
    if(error == QSerialPort::NoError) return;
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
        emit StatusChanged(false);
        // 预留: 可恢复errorOccurred信号链路给上层做专门故障处理。
        // emit errorOccurred(errorStr);
        emit logComm("ERROR", errorStr);
    }
}
