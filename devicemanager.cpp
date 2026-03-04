#include "devicemanager.h"
#include "serialworker.h"
#include "tcpworker.h"
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

DeviceManager::~DeviceManager(){
    teardownPipeline();
}

void DeviceManager::onFrameReceived(Frame frame){
    responseTimer.restart();

    if(state == DeviceState::Reconnecting){
        //emit logBusiness("网络波动已恢复", false);
        setState(DeviceState::Connected);
    }

    if(frame.funcCode == FUNC_TEMP_DATA){
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

void DeviceManager::requestOpen(int type,QString portName,int baudRate){
    setState(DeviceState::Connected);
    setupPipeline(type);
    emit signalOpen(portName,baudRate);
}

void DeviceManager::requestClose(){
    //先向子线程投递事件，再结束子线程（tearPipe）
    emit signalClose();
    qDebug()<<"device close";
    //setState(DeviceState::Disconnected);
    //teardownPipeline();
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
        //setState(DeviceState::Disconnected);
        requestClose();
        break;

    case DeviceState::Disconnected:
        timer->stop();
        timeoutTimer->stop();
        responseTimer.invalidate();
        //requestClose();
        break;

    }
}

void DeviceManager::setupPipeline(int type){

    //清理已有线程/对象
    teardownPipeline();

    thread = new QThread;//注意内存泄漏

    if(type == 0){
        worker = new SerialWorker;
    }else{
        worker = new TcpWorker;
    }

    parser = new ProtocolParser;
    worker->moveToThread(thread);
    parser->moveToThread(thread);

    //连接/断开连接
    connect(this,&DeviceManager::signalOpen,worker,&CommWorker::open);
    connect(this,&DeviceManager::signalClose,worker,&CommWorker::close);

    //发送链路
    connect(this,&DeviceManager::sendFrame,parser,&ProtocolParser::buildPacket);
    connect(parser,&ProtocolParser::sendRawData,worker,&CommWorker::sendData);

    //接收链路
    connect(worker,&CommWorker::rawDataReceived,parser,&ProtocolParser::onRawDataReceived);
    connect(parser,&ProtocolParser::frameReceived,this,&DeviceManager::onFrameReceived);

    connect(worker,&CommWorker::StatusChanged,this,[=](bool isOpen){
        if(isOpen){
            setState(DeviceState::Connected);
           // setupPipeline(type);
            emit statusChanged(true);
        }else{

            teardownPipeline();
            setState(DeviceState::Disconnected);
            emit statusChanged(false);
        }
    });

    //写日志：区分信号接力和信号连信号
    connect(worker,&CommWorker::rawDataReceived,this,[=](QByteArray rawdata){
        //if (ui->chkHexDisplay->isChecked()) {    //原始日志 (Hex View)
            QString hexLog = "原始数据: " + rawdata.toHex(' ').toUpper();
            emit logBusiness(hexLog, false);
       // }
    });
    connect(parser,&ProtocolParser::logProtocol, this, [=](QString msg, bool isSend){
        emit logBusiness("Parser: " + msg, isSend);
    });

    //错误
    connect(worker,&CommWorker::errorOccurred,this,[=](QString errorMsg){//指定this
       emit errorOccurred(errorMsg);
    });

    //线程结束后删除对象
    connect(thread, &QThread::finished, worker, &CommWorker::deleteLater);
    connect(thread, &QThread::finished, parser, &ProtocolParser::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}

void DeviceManager::teardownPipeline(){

    if (thread != nullptr && thread->isRunning()) {
        thread->quit(); // 请求退出事件循环
        thread->wait(); // 主线程等待子线程真正退出；线程结束了，不再用worker了，此时删它就安全
    }

    // 指针置空，防止野指针
    worker = nullptr;
    parser = nullptr;
    thread = nullptr;
}
