#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QObject>
#include "ProtocolData.h"

class ProtocolParser : public QObject
{
    Q_OBJECT
public:
    explicit ProtocolParser(QObject *parent = nullptr);

public slots:

    void onRawDataReceived(const QByteArray &rawdata);
    void onPackReadParam();
    void onPackWriteParam(const ConfigData &config);
    void onPackCmd();

signals:
    void sendRawData(const QByteArray &rawdata);//->Serial
    void RealtimeDataParsed(const DeviceData &data);//->Device 实时数据
    void configParamLoaded(const ConfigData &data); //->Device 参数配置
    void cmdAckReceived(bool ack, quint8 result);   //->Device 应答
    void logProtocol(const QString& level, const QString& message); //->Device

private:

    void processRawData();//解析数据
    void processFrame(const Frame &frame);
    void buildPacket(const Frame &frame);//
    QByteArray m_buffer;//全局接收缓冲区
    int m_readIndex=0;
};

#endif // PROTOCOLPARSER_H
