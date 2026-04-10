#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include "ProtocolData.h"
#include <QThread>
#include "protocolparser.h"
#include "commworker.h"
#include "databasemanager.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>

enum class DeviceState{
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

struct AlarmRule {
    QString paramName;  // 参数名：温/湿度
    QString unit;       // 单位：℃/%
    double upperLimit;  // 上限阈值
    double lowerLimit;  // 下限阈值
    bool isAlarming;    // 报警状态标志

    AlarmRule(QString name, QString u, double up, double low):
        paramName(name), unit(u), upperLimit(up), lowerLimit(low), isAlarming(false) {}
};

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager();
    DeviceData getLatestData(){
        QMutexLocker locker(&m_dataMutex);
        return m_latestData;
    }
    bool m_isTempSoftAlarm = false; // 温度是否软报警
    bool m_isHumSoftAlarm = false;  // 湿度是否软报警

public slots:
    void onRealtimeDataParsed(const DeviceData &newData);//<-Parser
    void onConfigParamLoaded(const ConfigData &data); //<-Parser
    void onCmdAckReceived(bool ack, quint8 errorCode); //<-Parser
  
    void requestReadParam(); //<-UI
    void requestWriteParam(const ConfigData &data); //<-UI
    void requestCmd(const QString &cmd); //<-UI
    void requestOpen(int type,QString portName,int baudRate);      //<-UI
    void requestClose();                                           //<-UI

signals:
    void signalOpen(QString target,int portOrBaud);     //->worker
    void signalClose();                                 //->worker
    void packReadParam(); //->Parser
    void packWriteParam(const ConfigData &config); //->Parser
    void packCmd(const QString &cmd); //->Parser

    void configReturned(const ConfigData &config);
    void logBusiness(const QString& level, const QString& message); //->UI

    void statusChanged(DeviceState state);                    //->UI
    void errorOccurred(QString errorMsg);               //->UI

    void sigSaveEnvData(double temp, double hum);
    void sigSaveEventLog(const QString &type, const QString &desc);
    void sigQueryDbHistory(const QDateTime& start, const QDateTime& end); //->db
    void sigDbHistoryReady(const QList<HistoryData>& dataList); //->UI

private:
    QTimer *timer = nullptr;        // 业务定时器（预留心跳/轮询扩展）
    QTimer *timeoutTimer = nullptr; // 检测定时器
    QElapsedTimer responseTimer;    //最新回复

    DeviceState state = DeviceState::Disconnected;
    int retryCount = 0;             // 自动重连计数

    DeviceData m_latestData;
    QMutex m_dataMutex;             // 跨线程数据保护锁，防止后续出现隐性竞态
    QTimer *m_dbSampleTimer = nullptr;        // 数据库降采样定时器

    QThread *workThread = nullptr;
    CommWorker *worker = nullptr;
    ProtocolParser *parser = nullptr;

    QThread *dbThread = nullptr;
    DatabaseManager *dbManager = nullptr;

    void setState(DeviceState newState);
    void tryAutoReconnect(const QString &reason); // 自动重连逻辑
    void setupPipeline(int type);
    void teardownPipeline();

    void checkSoftAlarm(double currentValue, AlarmRule &rule);// 软报警检测
    AlarmRule m_tempRule{"温度", "℃", 8.0, 2.0};
    AlarmRule m_humRule{"湿度", "%", 75.0, 35.0};

    // 连接配置记录，用于自动重连
    int m_lastConnType = 0; // 0=串口, 1=TCP
    QString m_lastTarget;   
    int m_lastPortOrBaud = 0;
    bool m_hasConnProfile = false; // 是否有连接配置
    bool m_autoReconnectEnabled = false; // 是否启用自动重连

};

#endif // DEVICEMANAGER_H
