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

        if(state==DeviceState::Connected || state==DeviceState::Reconnecting){
            //QByteArray queryCmd = QByteArray::fromHex("AA 55 03 00 03 FF");
            onSendData(FUNC_READ_TEMP,QByteArray());
            // 发送日志
            emit logBusiness("Heartbeat sent!",true); // true表示发送
        }
    });

    connect(timeoutTimer,&QTimer::timeout,[=](){
        if(!responseTimer.isValid()) return;
        if(responseTimer.elapsed() > 2000){
            setState(DeviceState::Reconnecting);
            retryCount++;
            qDebug() << QString("检测到超时！正在重连...%1/5...").arg(retryCount);
            if(retryCount>5){
                setState(DeviceState::Error);
            }
        }
    });
}

void DeviceManager::onFrameReceived(Frame frame){
    responseTimer.restart();

    if(state == DeviceState::Reconnecting){
        //emit logBusiness("网络波动已恢复", false);
        setState(DeviceState::Connected);
    }

    if(frame.funcCode == FUNC_TEMP_DATA){//整型提升quint8->char?
        if(frame.payload.size()>=2){
            //1B 温度值 uint8_t
            unsigned char highByte = static_cast<unsigned char>( frame.payload.at(0));//?-3°C?
            unsigned char lowByte = static_cast<unsigned char>( frame.payload.at(1));//?-3°C?
            short temp = (highByte<<8) | lowByte;//int16_t
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
        setState(DeviceState::Connected);
    }else{
        setState(DeviceState::Disconnected);
    }
}

//统一管理状态切换
void DeviceManager::setState(DeviceState newState){
    if(state == newState) return;
    state = newState;

    switch(newState){
    case DeviceState::Connected:
        timer->start();
        timeoutTimer->start();
        responseTimer.start();
        retryCount=0;//
        qDebug()<<"设备已在线";
        break;

    case DeviceState::Connecting:
        qDebug()<<"正在连接串口...";
        break;

    case DeviceState::Reconnecting:
        qDebug()<<"连接不稳定，尝试恢复...";
        break;

    case DeviceState::Error:
        timeoutTimer->stop();//顺序
        timer->stop();
        emit logBusiness("设备已离线 (最大重试次数已满)",false);//
        setState(DeviceState::Disconnected);
        break;

    case DeviceState::Disconnected:
        timer->stop();
        timeoutTimer->stop();
        responseTimer.invalidate();
        emit deviceOffline();
        break;

    }
}
