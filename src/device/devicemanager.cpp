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

    // 预留扩展: 定时心跳包（当前采用超时检测+重连）。
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
            tryAutoReconnect("通信超时");
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

    // 定时采样写库，避免每帧都触发数据库I/O。
    connect(m_dbSampleTimer,&QTimer::timeout,this,[=](){

        if(state != DeviceState::Connected || m_latestData.actualTemperature == 0) return;
        QMutexLocker locker(&m_dataMutex);
        emit sigSaveEnvData(m_latestData.actualTemperature, m_latestData.actualHumidity);
    });

}

DeviceManager::~DeviceManager(){
    teardownPipeline();

    if (dbThread != nullptr && dbThread->isRunning()) {
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

    responseTimer.restart();
    if(state == DeviceState::Connecting || state == DeviceState::Reconnecting){
        setState(DeviceState::Connected);
    }

    QMutexLocker locker(&m_dataMutex);

    // 离散变量：状态突变检测
    // 门禁状态
    if (m_latestData.doorStatus != newData.doorStatus) {

         qWarning() << "[系统事件] 箱门状态发生改变，当前:" << newData.doorStatus;
        QString statusStr = (newData.doorStatus == 1) ? "被打开" : "已关闭";
        QString level = (newData.doorStatus == 1)? "WARNING" : "INFO";
        QString logMsg = QString("冷藏箱门%1").arg(statusStr);

        emit logBusiness(level, logMsg); // 记录日志
        emit sigSaveEventLog("DOOR_EVENT", logMsg); // 录入审计数据库
    }
    // 压缩机状态
    if (m_latestData.compressorStatus != newData.compressorStatus) {

        qWarning() << "[系统事件] 压缩机状态发生改变，当前:" << newData.compressorStatus;

        QString statusStr = (newData.compressorStatus == 1) ? "启动制冷" : "停止待机";
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

    responseTimer.restart();// 重启定时器
    if(state == DeviceState::Connecting || state == DeviceState::Reconnecting){ // 恢复连接
        setState(DeviceState::Connected);
    }

    emit configReturned(data);
}

void DeviceManager::onCmdAckReceived(bool ack, quint8 errorCode){

    responseTimer.restart();
    if(state == DeviceState::Connecting || state == DeviceState::Reconnecting){
        setState(DeviceState::Connected);
    }

    if (ack) {
        qDebug() << "底层执行成功！";//目前只在参数下发时下位机会回复ACK
        // m_tempRule = {"温度", "℃", data.tempHighLimit, data.tempLowLimit};
        // m_humRule = {"湿度", "%", data.humidHighLimit, data.humidLowLimit};
        // emit logBusiness("INFO", "成功修改参数配置");// 应该等到下位机响应才记录日志和审计表
        // emit sigSaveEventLog("SYS_EVENT", "成功修改参数配置");
        emit logBusiness("INFO", "指令执行成功");
    } else {
        QString msg = QString("设置失败，底层拒绝执行，错误码: %1").arg(errorCode);
        emit logBusiness("ERROR", msg);
    }
}


void DeviceManager::requestReadParam(){
    qDebug()<<"读取参数指令下发";
    emit packReadParam();
}

void DeviceManager::requestWriteParam(const ConfigData &data){
    QString msg = QString("准备修改参数配置：%1 %2 %3 %4 %5 %6")//
                      .arg(data.targetTemperature)
                      .arg(data.tempHighLimit)
                      .arg(data.tempLowLimit)
                      .arg(data.targetHumidity)
                      .arg(data.humidHighLimit)
                      .arg(data.humidLowLimit);
    qDebug()<< msg;
    emit packWriteParam(data);

    // ！应该等到下位机响应才更改告警规则并记录日志和审计表
    m_tempRule = {"温度", "℃", data.tempHighLimit, data.tempLowLimit};
    m_humRule = {"湿度", "%", data.humidHighLimit, data.humidLowLimit};
    emit logBusiness("INFO", "成功修改参数配置");
    emit sigSaveEventLog("SYS_EVENT", "成功修改参数配置");
}

void DeviceManager::requestCmd(const QString &cmd){
    emit packCmd(cmd);
}

void DeviceManager::requestOpen(int type,QString portName,int baudRate){
    // 缓存最近连接参数，供自动重连复用
    m_lastConnType = type;
    m_lastTarget = portName;
    m_lastPortOrBaud = baudRate;
    m_hasConnProfile = true; 
    m_autoReconnectEnabled = true;

    setupPipeline(type);
    setState(DeviceState::Connecting); // 连接成功后在解析回包时切换为Connected
    emit signalOpen(portName,baudRate);
}

void DeviceManager::requestClose(){
    m_autoReconnectEnabled = false;
    // 先通知底层关闭，再由状态链路收敛到Disconnected。
    emit signalClose();
    qDebug()<<"device close";
}

// 统一管理状态切换
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
        retryCount=0;
        requestReadParam(); // 每次连接后主动拉取一次参数，保证UI与设备配置一致
        qDebug()<<"设备已在线";
        emit logBusiness("INFO", "设备已在线");
        break;

    case DeviceState::Connecting:
        timeoutTimer->start();
        responseTimer.start();
        qDebug()<<"正在连接设备...";
        emit logBusiness("INFO", "正在连接设备...");
        break;

    case DeviceState::Reconnecting:
        qDebug()<<"连接不稳定，尝试恢复...";
        emit logBusiness("WARNING", "连接不稳定，尝试恢复...");
        break;

    case DeviceState::Error:
        m_autoReconnectEnabled = false;
        timeoutTimer->stop();
        timer->stop();
        m_dbSampleTimer->stop();
        emit logBusiness("ERROR", "设备未启动或已离线");
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

void DeviceManager::tryAutoReconnect(const QString &reason)
{
    if (!m_autoReconnectEnabled || !m_hasConnProfile) {
        setState(DeviceState::Error);
        return;
    }

    retryCount++;
    if (retryCount > 5) {
        m_autoReconnectEnabled = false;
        setState(DeviceState::Error);
        emit logBusiness("ERROR", "自动重连失败，已超过最大重试次数");
        return;
    }

    setState(DeviceState::Reconnecting);
    emit logBusiness("WARNING", QString("%1，正在自动重连...%2/5").arg(reason).arg(retryCount));

    requestOpen(m_lastConnType, m_lastTarget, m_lastPortOrBaud);
    responseTimer.restart();
}

void DeviceManager::setupPipeline(int type){

    // 清理已有线程/对象，避免重复建链
    teardownPipeline();

    workThread = new QThread();

    if(type == 0){
        worker = new SerialWorker;
    }else{
        worker = new TcpWorker;
    }
    parser = new ProtocolParser;

    worker->moveToThread(workThread);
    parser->moveToThread(workThread);

    // 连接/断开链路
    connect(this,&DeviceManager::signalOpen,worker,&CommWorker::open);
    connect(this,&DeviceManager::signalClose,worker,&CommWorker::close);

    //发送链路
    //connect(this,&DeviceManager::sendFrame,parser,&ProtocolParser::buildPacket);
    connect(this,&DeviceManager::packReadParam,parser,&ProtocolParser::onPackReadParam);
    connect(this,&DeviceManager::packWriteParam,parser,&ProtocolParser::onPackWriteParam);
    connect(this,&DeviceManager::packCmd,parser,&ProtocolParser::onPackCmd);
    connect(parser,&ProtocolParser::sendRawData,worker,&CommWorker::sendData);

    // 接收链路
    connect(worker,&CommWorker::rawDataReceived,parser,&ProtocolParser::onRawDataReceived);
    //connect(parser,&ProtocolParser::RealtimeDataParsed,this,&DeviceManager::onRealtimeDataParsed);
    connect(parser,&ProtocolParser::RealtimeDataParsed,this,&DeviceManager::onRealtimeDataParsed);
    connect(parser,&ProtocolParser::configParamLoaded,this,&DeviceManager::onConfigParamLoaded);
    connect(parser,&ProtocolParser::cmdAckReceived,this,&DeviceManager::onCmdAckReceived);

    // 状态链路
    connect(worker,&CommWorker::StatusChanged,this,[=](bool isOpen){
        if(isOpen){
            setState(DeviceState::Connecting);
        }else{
            if (m_autoReconnectEnabled && state != DeviceState::Disconnected) {
                tryAutoReconnect("连接断开");
            } else {
                teardownPipeline();
                setState(DeviceState::Disconnected);
            }
        }
    });

    // 日志链路
    connect(worker, &CommWorker::logComm, this, &DeviceManager::logBusiness);
    connect(parser, &ProtocolParser::logProtocol, this, &DeviceManager::logBusiness);

    // 预留扩展: 独立错误信号链路（与日志链路分离）
    // connect(worker,&CommWorker::errorOccurred,this,[=](QString errorMsg){//指定this
    //    emit errorOccurred(errorMsg);
    // });

    // 线程结束后删除对象
    connect(workThread, &QThread::finished, worker, &CommWorker::deleteLater);
    connect(workThread, &QThread::finished, parser, &ProtocolParser::deleteLater);
    connect(workThread, &QThread::finished, workThread, &QThread::deleteLater);

    workThread->start();
}

void DeviceManager::teardownPipeline(){

    if (workThread != nullptr && workThread->isRunning()) {
        workThread->quit();
        workThread->wait();
    }

    // 指针置空，避免悬挂引用
    worker = nullptr;
    parser = nullptr;
    workThread = nullptr;
}
