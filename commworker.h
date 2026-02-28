#ifndef COMMWORKER_H
#define COMMWORKER_H

#include <QObject>

class CommWorker : public QObject
{
    Q_OBJECT
public:
    explicit CommWorker(QObject *parent = nullptr);
    virtual ~CommWorker(){} //虚析构函数

public slots:
    virtual void open(QString target,int portOrBaud) = 0;//<-UI 纯虚函数
    virtual void close() = 0;//<-UI
    virtual void sendData(const QByteArray &data) = 0;//<-Parser

signals:
    void StatusChanged(bool isOpen);//->UI
    void errorOccurred(QString errorMsg);//->UI
    void rawDataReceived(const QByteArray &rawdata);//->Parser

//private:
    //QSerialPort *serial;//串口对象
};

#endif // COMMWORKER_H
