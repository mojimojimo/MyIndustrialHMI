#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include "ProtocolData.h"
#include <QTimer>
#include <QElapsedTimer>

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);

public slots:
    void onFrameReceived(Frame frame);//<-Parser
    void onSendData(char funcCode, const QByteArray &dataContent);
    void startDevice(bool toStart);//<-UI

signals:
    void sendFrame(Frame frame);//->Parser
    void dataReceived(int type,double value);//->UI
    void deviceOffline();
    void logBusiness(const QString &text, bool isSend);

private:
    QTimer *timer = nullptr;
    QTimer *timeoutTimer = nullptr;
    QElapsedTimer responseTimer;
};

#endif // DEVICEMANAGER_H
