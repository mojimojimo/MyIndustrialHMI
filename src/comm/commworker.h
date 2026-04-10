#ifndef COMMWORKER_H
#define COMMWORKER_H

#include <QObject>

class CommWorker : public QObject
{
    Q_OBJECT
public:
    explicit CommWorker(QObject *parent = nullptr);
    virtual ~CommWorker(){} // 保证子类通过基类指针析构安全

public slots:
    virtual void open(QString target,int portOrBaud) = 0;// <-device 纯虚函数
    virtual void close() = 0; // <-device
    virtual void sendData(const QByteArray &data) = 0;//<-Parser

signals:
    void StatusChanged(bool isOpen); //->device
    //void errorOccurred(QString errorMsg); //->device
    void rawDataReceived(const QByteArray &rawdata);//->Parser
    void logComm(const QString& level, const QString& message);// ->device

};

#endif // COMMWORKER_H
