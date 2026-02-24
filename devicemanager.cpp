#include "devicemanager.h"
#include <QDebug>
#include <QDateTime>

DeviceManager::DeviceManager(QObject *parent)
    : QObject{parent}
{
    timer = new QTimer(this);
    timeoutTimer = new QTimer(this);
    timer->setInterval(1000);
    timeoutTimer->setInterval(500);

    //定时发送心跳包
    connect(timer,&QTimer::timeout,[=](){

        //QByteArray queryCmd = QByteArray::fromHex("AA 55 03 00 03 FF");
        onSendData(FUNC_READ_TEMP,QByteArray());
        // 发送日志
        emit logBusiness("Heartbeat sent!",true); // true表示发送

    });

    connect(timeoutTimer,&QTimer::timeout,[=](){
        if(!responseTimer.isValid()) return;
        if(responseTimer.elapsed()>2000){
            qDebug() << "检测到超时！准备断开...";
            //顺序
            timeoutTimer->stop();
            timer->stop();
            emit deviceOffline();
            QString errorMsg ="设备已下线！（超时未响应）";
            //QMessageBox::critical(this,"错误", errorMsg);
            emit logBusiness(errorMsg,false);//

            responseTimer.invalidate();
        }
    });
}

void DeviceManager::onFrameReceived(Frame frame){
    responseTimer.restart();
    if(frame.funcCode == FUNC_TEMP_DATA){//整型提升quint8->char?
        if(frame.payload.size()>=2){
            //1B 温度值
            unsigned char highByte = static_cast<unsigned char>( frame.payload.at(0));//?-3°C?
            unsigned char lowByte = static_cast<unsigned char>( frame.payload.at(1));//?-3°C?
            short temp = (highByte<<8) | lowByte;
            double realTemp = temp / 10.0;
            emit dataReceived(1,realTemp);
            qDebug()<<"温度:"<<realTemp<<" ℃ ";
        }
    }else if(frame.funcCode == 0x02){
        qDebug()<<"电机转速:";
    }
}

void DeviceManager::onSendData(char funcCode, const QByteArray &dataContent){
    Frame frame;
    frame.funcCode = funcCode;
    frame.payload = dataContent;
    emit sendFrame(frame);
}

void DeviceManager::startDevice(bool toStart){
    if(toStart){
        timer->start();
        timeoutTimer->start();
        responseTimer.start(); //给设备2秒的反应时间
    }else{
        timer->stop();
        timeoutTimer->stop();
        responseTimer.invalidate();
    }
}
