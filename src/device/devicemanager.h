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

public slots:
    void onRealtimeDataParsed(const DeviceData &newData);//<-Parser
    void onConfigParamLoaded(const ConfigData &data); //<-Parser
    void onCmdAckReceived(bool ack, quint8 errorCode); //<-Parser
    void onSendData(char funcCode, const QByteArray &dataContent); //<-UI
    void requestOpen(int type,QString portName,int baudRate);      //<-UI
    void requestClose();                                           //<-UI
    //void setAlarmThresholds(double lower, double upper, bool isTemp);   //<-UI

signals:
    void signalOpen(QString target,int portOrBaud);     //->worker
    void signalClose();                                 //->worker
    void sendFrame(const Frame &frame);                 //->Parser
    //void dataReceived(const DeviceData &data);          //->UI
    void logBusiness(const QString &text, bool isSend); //->UI
    void statusChanged(bool isOpen);                    //->UI
    void errorOccurred(QString errorMsg);               //->UI

    void sigSaveEnvData(double temp, double hum);
    void sigSaveEventLog(const QString &type, const QString &desc);

private:
    QTimer *timer = nullptr;        //心跳定时器
    QTimer *timeoutTimer = nullptr; //检测定时器
    QElapsedTimer responseTimer;    //最新回复

    DeviceState state = DeviceState::Disconnected;
    int retryCount = 0;             //重连次数

    DeviceData m_latestData;
    QMutex m_dataMutex;             // 跨线程数据保护锁
    QMutex m_configMutex;           // 参数配置保护锁
    QTimer *m_dbSampleTimer = nullptr;        // 数据库降采样定时器

    QThread *workThread = nullptr;
    CommWorker *worker = nullptr;
    ProtocolParser *parser = nullptr;

    QThread *dbThread = nullptr;
    DatabaseManager *dbManager = nullptr;

    void setState(DeviceState newState);
    void setupPipeline(int type);
    void teardownPipeline();

    void checkSoftAlarm(double currentValue, AlarmRule &rule);// 软报警检测
    AlarmRule m_tempRule{"温度", "℃", 8.0, 2.0};
    AlarmRule m_humRule{"湿度", "%", 75.0, 35.0};

};

#endif // DEVICEMANAGER_H
