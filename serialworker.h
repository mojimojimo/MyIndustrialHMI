#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>

class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker();


public slots:
    void openSerialPort(QString portName,int baudRate);
    void closeSerialPort();
    void sendData(const QByteArray &data);//<-Parser

private slots:
    void onReadyRead();
    void handleError(QSerialPort::SerialPortError error);//硬件检测

signals:
    void portStatusChanged(bool isOpen);
    void errorOccuerred(QString errorMsg);//->UI
    void rawDataReceived(const QByteArray &rawdata);//->Parser

private:
    QSerialPort *serial;//串口对象
};

#endif // SERIALWORKER_H
