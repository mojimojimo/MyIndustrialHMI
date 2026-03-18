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
    void onSendData(char funcCode, const QByteArray &dataContent); //<-UI
    void requestOpen(int type,QString portName,int baudRate);      //<-UI
    void requestClose();                                           //<-UI

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
    QTimer *m_dbSampleTimer = nullptr;        // 数据库降采样定时器

    QThread *workThread = nullptr;
    CommWorker *worker = nullptr;
    ProtocolParser *parser = nullptr;

    QThread *dbThread = nullptr;
    DatabaseManager *dbManager = nullptr;

    void setState(DeviceState newState);
    void setupPipeline(int type);
    void teardownPipeline();

};

#endif // DEVICEMANAGER_H
