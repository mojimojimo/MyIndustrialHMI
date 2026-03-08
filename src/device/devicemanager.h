#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include "ProtocolData.h"
#include <QThread>
#include "protocolparser.h"
#include "commworker.h"
#include <QTimer>
#include <QElapsedTimer>

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

public slots:
    void onFrameReceived(const Frame &frame);//<-Parser
    void onSendData(char funcCode, const QByteArray &dataContent);//<-UI
    void requestOpen(int type,QString portName,int baudRate);//<-UI
    void requestClose();//<-UI

signals:
    void signalOpen(QString target,int portOrBaud);//->worker
    void signalClose();//->worker
    void sendFrame(const Frame &frame);//->Parser
    void dataReceived(int type,double value);//->UI
    void logBusiness(const QString &text, bool isSend);//->UI
    void statusChanged(bool isOpen);//->UI
    void errorOccurred(QString errorMsg);

private:
    QTimer *timer = nullptr;
    QTimer *timeoutTimer = nullptr;
    QElapsedTimer responseTimer;
    DeviceState state = DeviceState::Disconnected;
    int retryCount = 0;

    QThread *thread = nullptr;
    CommWorker *worker = nullptr;
    ProtocolParser *parser = nullptr;

    void setState(DeviceState newState);
    void setupPipeline(int type);
    void teardownPipeline();

};

#endif // DEVICEMANAGER_H
