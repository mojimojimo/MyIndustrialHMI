#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QObject>
#include "serialworker.h"
#include "ProtocolData.h"

class ProtocolParser : public QObject
{
    Q_OBJECT
public:
    explicit ProtocolParser(QObject *parent = nullptr);

public slots:
    void buildPacket(Frame frame);//
    void onRawDataReceived(QByteArray rawdata);

signals:
    void sendRawData(QByteArray rawdata);//->Serial
    void frameReceived(Frame frame);//->Device
    void logProtocol(const QString &text,bool isSend);//->UI

private:

    void processData();//解析数据
    QByteArray m_buffer;//全局接收缓冲区
};

#endif // PROTOCOLPARSER_H
