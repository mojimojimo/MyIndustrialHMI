#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QSerialPort>
#include "commworker.h"

class SerialWorker : public CommWorker
{
    Q_OBJECT
public:
    explicit SerialWorker(CommWorker *parent = nullptr);
    ~SerialWorker();


public slots:
    void open(QString target,int portOrBaud) override;//<-UI
    void close() override;//<-UI
    void sendData(const QByteArray &data) override;//<-Parser

private slots:
    void onReadyRead();
    void handleError(QSerialPort::SerialPortError error);//硬件检测

// signals:
//     void StatusChanged(bool isOpen);//->UI
//     void errorOccuerred(QString errorMsg);//->UI
//     void rawDataReceived(const QByteArray &rawdata);//->Parser

private:
    QSerialPort *serial;//串口对象
};

#endif // SERIALWORKER_H
