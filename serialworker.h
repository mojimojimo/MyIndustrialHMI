#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include "ProtocolData.h"

class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker();


public slots://?
    void openSerialPort(QString portName,int baudRate);
    void closeSerialPort();
    void sendData(QByteArray data);

private slots:
    void onReadyRead();
    void handleError(QSerialPort::SerialPortError error);

signals://?
    void portStatusChanged(bool isOpen);
    void errorOccuerred(QString errorMsg);
    void dataReceived(int type,double value);
    void rawDataReceived(QString rawdata);//

private:
    QSerialPort *serial;//串口对象
    QByteArray m_buffer;//全局接收缓冲区
    void processData();
};

#endif // SERIALWORKER_H
