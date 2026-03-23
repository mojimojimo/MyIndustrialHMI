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
    m_dbSampleTimer = new QTimer(this);
    timer->setInterval(1000);
    timeoutTimer->setInterval(500);
    m_dbSampleTimer->setInterval(1000);

    // //定时发送心跳包
    // connect(timer,&QTimer::timeout,this,[=](){

    //     if(state==DeviceState::Connected || state==DeviceState::Reconnecting){
    //         //QByteArray queryCmd = QByteArray::fromHex("AA 55 03 00 03 FF");
    //         onSendData(0x00,QByteArray());
    //         // 发送日志
    //         emit logBusiness("INFO","Heartbeat sent!"); // true表示发送
    //     }
    // });

    connect(timeoutTimer,&QTimer::timeout,this,[=](){
        if(!responseTimer.isValid()) return;
        if(responseTimer.elapsed() > 2000){
            setState(DeviceState::Reconnecting);
            retryCount++;
            emit logBusiness("WARNING", QString("检测到超时！正在重连...%1/5...").arg(retryCount));
            if(retryCount>5){
                setState(DeviceState::Error);
            }
        }
    });

    dbThread = new QThread(this);
    dbManager = new DatabaseManager;
    dbManager->moveToThread(dbThread);

    //初始化数据库连接
    connect(dbThread, &QThread::started, dbManager, &DatabaseManager::init);

    //存储链路
    connect(this, &DeviceManager::sigSaveEnvData, dbManager, &DatabaseManager::onInsertEnvData);
    connect(this, &DeviceManager::sigSaveEventLog, dbManager, &DatabaseManager::onInsertEvent);
    //查询与接收历史数据
    connect(this, &DeviceManager::sigQueryDbHistory, dbManager, &DatabaseManager::onQueryHistory);
    connect(dbManager, &DatabaseManager::sigHistoryDataReady, this, &DeviceManager::sigDbHistoryReady);

    connect(dbThread, &QThread::finished, dbManager, &DatabaseManager::deleteLater);

    dbThread->start();

    //定时采样数据存入db
    connect(m_dbSampleTimer,&QTimer::timeout,this,[=](){

        if(state != DeviceState::Connected || m_latestData.actualTemperature == 0) return;//加一个判断
        QMutexLocker locker(&m_dataMutex);
        emit sigSaveEnvData(m_latestData.actualTemperature, m_latestData.actualHumidity);
    });

}

DeviceManager::~DeviceManager(){
    teardownPipeline();

    if (dbThread != nullptr && dbThread->isRunning()) {//！
        dbThread->quit();
        dbThread->wait();
    }

    dbManager = nullptr;
}

void DeviceManager::checkSoftAlarm(double currentValue, AlarmRule& rule) {
    bool isOut = (currentValue > rule.upperLimit || currentValue < rule.lowerLimit);

    if (isOut && !rule.isAlarming) {
        // 刚刚越界，触发报警
        QString msg = QString("【软报警】%1越界！当前: %2 %3 (安全范围: %4~%5)")
                          .arg(rule.paramName)
                          .arg(currentValue)
                          .arg(rule.unit)
                          .arg(rule.lowerLimit)
                          .arg(rule.upperLimit);

        emit logBusiness("ERROR", msg);
        emit sigSaveEventLog("SOFT_ALARM", msg);
        rule.isAlarming = true;

    } else if (!isOut && rule.isAlarming) {
        // 数值恢复正常，报警解除
        QString msg = QString("%1已恢复至安全区间。").arg(rule.paramName);
        emit logBusiness("INFO", msg);
        emit sigSaveEventLog("ALARM_CLEAR", msg);
        rule.isAlarming = false;
    }
}

void DeviceManager::onRealtimeDataParsed(const DeviceData &newData){

    responseTimer.restart();// 重启定时器
    // qDebug()<<"door"<<newData.doorStatus;
    // qDebug()<<"temp"<<newData.actualTemperature;
    // qDebug()<<"hum"<<newData.actualHumidity;
    // qDebug()<<"alarm"<<newData.alarmCode;
    // qDebug()<<"compress"<<newData.compressorStatus;
    if(state == DeviceState::Reconnecting){ // 恢复连接
        emit logBusiness("INFO", "网络波动已恢复");
        setState(DeviceState::Connected);
    }

    QMutexLocker locker(&m_dataMutex);

    // 离散变量：状态突变检测
    // 门禁状态
    if (m_latestData.doorStatus != newData.doorStatus) {

         qWarning() << "[系统事件] 箱门状态发生改变，当前:" << newData.doorStatus;
        QString statusStr = (newData.doorStatus == 1)? "被打开" :"已关闭";
        QString level = (newData.doorStatus == 1)? "WARNING" : "INFO";
        QString logMsg = QString("冷藏箱门%1").arg(statusStr);

        emit logBusiness(level, logMsg); // 记录日志
        emit sigSaveEventLog("DOOR_EVENT", logMsg); // 录入审计数据库
    }
    // 压缩机状态
    if (m_latestData.compressorStatus != newData.compressorStatus) {

        qWarning() << "[系统事件] 压缩机状态发生改变，当前:" << newData.compressorStatus;

        QString statusStr = (newData.compressorStatus == 1)? "启动制冷" : "停止待机";
        QString logMsg = QString("压缩机%1").arg(statusStr);

        emit logBusiness("INFO",logMsg); // 记录日志
        emit sigSaveEventLog("SYS_EVENT", logMsg); // 录入审计数据库
    }
    // 报警码
    if (m_latestData.alarmCode != newData.alarmCode) {
        qWarning() << "[警报] 报警码变更！旧:" << m_latestData.alarmCode << " 新:" << newData.alarmCode;
        if(newData.alarmCode != 0){ // 产生报警
             QString logMsg = QString("触发系统级报警！故障码: %1").arg(newData.alarmCode);
            emit logBusiness("ERROR", logMsg);
            emit sigSaveEventLog("ALARM", logMsg);
        } else {  // 报警解除
            emit logBusiness("INFO", "系统报警已解除");
            emit sigSaveEventLog("ALARM_CLEAR", "系统报警已解除");
        }
    }

    // 温/湿度越界检测
    checkSoftAlarm(newData.actualTemperature, m_tempRule); // 查温度
    m_isTempSoftAlarm = m_tempRule.isAlarming;
    checkSoftAlarm(newData.actualHumidity, m_humRule);     // 查湿度
    m_isHumSoftAlarm = m_humRule.isAlarming;

    m_latestData = newData;// 连续变量：覆盖更新
}

void DeviceManager::onConfigParamLoaded(const ConfigData &data){
    QMutexLocker locker(&m_configMutex);
    QString msg = QString("修改参数配置：%1 %2 %3 %4 %5 %6")
                      .arg(data.targetTemperature)
                      .arg(data.tempHighLimit)
                      .arg(data.tempLowLimit)
                      .arg(data.targetHumidity)
                      .arg(data.humidHighLimit)
                      .arg(data.humidLowLimit);
    m_tempRule = {"温度", "℃", data.tempHighLimit, data.tempLowLimit};
    m_humRule = {"湿度", "%", data.humidHighLimit, data.humidLowLimit};
    qDebug()<< msg;
    emit logBusiness("INFO", "成功修改参数配置");
    emit sigSaveEventLog("SYS_EVENT", "成功修改参数配置");
}

void DeviceManager::onCmdAckReceived(bool ack, quint8 errorCode){
    if (ack) {
        qDebug() << "底层执行成功！";
    } else {
        QString msg = QString("设置失败，底层拒绝执行，错误码: %1").arg(errorCode);
        emit logBusiness("ERROR", msg);
    }
}

// void DeviceManager::onSendData(char funcCode, const QByteArray &dataContent){
//     Frame frame;
//     frame.funcCode = funcCode;
//     frame.payload = dataContent;
//     emit sendFrame(frame);
// }

void DeviceManager::requestReadParam(){
    QMutexLocker locker(&m_configMutex);
    emit packReadParam();
}

void DeviceManager::requestWriteParam(const ConfigData &config){
    QMutexLocker locker(&m_configMutex);
    emit packWriteParam(config);
}

void DeviceManager::requestCmd(){//default强制消音
    emit packCmd();
}

void DeviceManager::requestOpen(int type,QString portName,int baudRate){
    setState(DeviceState::Connecting);//成功连接才设置Connected
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
    emit statusChanged(newState);

    switch(newState){
    case DeviceState::Connected:
        timer->start();
        timeoutTimer->start();
        responseTimer.start();
        m_dbSampleTimer->start();
        retryCount=0;//
        qDebug()<<"设备已在线";
        emit logBusiness("INFO", "设备已在线");
        break;

    case DeviceState::Connecting:
        qDebug()<<"正在连接串口...";
        emit logBusiness("INFO", "正在连接串口...");
        break;

    case DeviceState::Reconnecting:
        qDebug()<<"连接不稳定，尝试恢复...";
        emit logBusiness("WARNING", "连接不稳定，尝试恢复...");
        break;

    case DeviceState::Error:
        timeoutTimer->stop();//顺序
        timer->stop();
        m_dbSampleTimer->stop();
        emit logBusiness("ERROR", "设备已离线 (最大重试次数已满)");//
        requestClose();
        break;

    case DeviceState::Disconnected:
        timer->stop();
        timeoutTimer->stop();
        responseTimer.invalidate();
        m_dbSampleTimer->stop();
        emit logBusiness("INFO", "设备已离线");
        break;

    }
}

void DeviceManager::setupPipeline(int type){

    //清理已有线程/对象
    teardownPipeline();

    workThread = new QThread();//注意内存泄漏

    if(type == 0){
        worker = new SerialWorker;
    }else{
        worker = new TcpWorker;
    }
    parser = new ProtocolParser;

    worker->moveToThread(workThread);
    parser->moveToThread(workThread);

    //连接/断开连接
    connect(this,&DeviceManager::signalOpen,worker,&CommWorker::open);
    connect(this,&DeviceManager::signalClose,worker,&CommWorker::close);

    //发送链路
    //connect(this,&DeviceManager::sendFrame,parser,&ProtocolParser::buildPacket);
    connect(this,&DeviceManager::packReadParam,parser,&ProtocolParser::onPackReadParam);
    connect(this,&DeviceManager::packWriteParam,parser,&ProtocolParser::onPackWriteParam);
    connect(this,&DeviceManager::packCmd,parser,&ProtocolParser::onPackCmd);
    connect(parser,&ProtocolParser::sendRawData,worker,&CommWorker::sendData);

    //接收链路
    connect(worker,&CommWorker::rawDataReceived,parser,&ProtocolParser::onRawDataReceived);
    //connect(parser,&ProtocolParser::RealtimeDataParsed,this,&DeviceManager::onRealtimeDataParsed);
    connect(parser,&ProtocolParser::RealtimeDataParsed,this,&DeviceManager::onRealtimeDataParsed,Qt::DirectConnection);
    connect(parser,&ProtocolParser::configParamLoaded,this,&DeviceManager::onConfigParamLoaded,Qt::DirectConnection);
    connect(parser,&ProtocolParser::cmdAckReceived,this,&DeviceManager::onCmdAckReceived);

    //状态链路
    connect(worker,&CommWorker::StatusChanged,this,[=](bool isOpen){
        if(isOpen){
            setState(DeviceState::Connected);
           // setupPipeline(type);
            //emit statusChanged(true);
        }else{

            teardownPipeline();
            setState(DeviceState::Disconnected);
            //emit statusChanged(false);
        }
    });

    //日志链路
    connect(worker, &CommWorker::logComm, this, &DeviceManager::logBusiness);
    connect(parser, &ProtocolParser::logProtocol, this, &DeviceManager::logBusiness);

    //错误
    // connect(worker,&CommWorker::errorOccurred,this,[=](QString errorMsg){//指定this
    //    emit errorOccurred(errorMsg);
    // });

    //线程结束后删除对象
    connect(workThread, &QThread::finished, worker, &CommWorker::deleteLater);
    connect(workThread, &QThread::finished, parser, &ProtocolParser::deleteLater);
    connect(workThread, &QThread::finished, workThread, &QThread::deleteLater);

    workThread->start();
}

void DeviceManager::teardownPipeline(){

    if (workThread != nullptr && workThread->isRunning()) {
        workThread->quit(); // 请求退出事件循环
        workThread->wait(); // 主线程等待子线程真正退出；线程结束了，不再用worker了，此时删它就安全
    }

    // 指针置空，防止野指针
    worker = nullptr;
    parser = nullptr;
    workThread = nullptr;
}
