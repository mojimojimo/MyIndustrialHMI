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
    void onPackCmd(const QString &cmd);

signals:
    void sendRawData(const QByteArray &rawdata);           // -> CommWorker
    void RealtimeDataParsed(const DeviceData &data);       // -> DeviceManager 实时数据
    void configParamLoaded(const ConfigData &data);        // -> DeviceManager 参数配置
    void cmdAckReceived(bool ack, quint8 result);          // -> DeviceManager 指令应答
    void logProtocol(const QString& level, const QString& message);

private:

    void processRawData();//解析数据
    void processFrame(const Frame &frame);
    void buildPacket(const Frame &frame);//

    // Ring buffer helpers
    void resetRingBuffer();
    int ringAvailable() const;                 // 当前可读字节数
    int ringFreeSpace() const;                 // 剩余可写空间
    int ringTailIndex() const;                 // 逻辑写指针位置
    char ringPeek(int offset) const;           // 相对head读取单字节(自动回绕)
    bool ringWrite(const QByteArray &data);    // 追加写入(支持跨尾分段)
    bool ringDrop(int count);                  // 消费count字节并推进head
    QByteArray ringReadSlice(int offset, int length) const; // 按逻辑区间读取连续切片

    static constexpr int RING_BUFFER_CAPACITY = 64 * 1024;
    QByteArray m_ringBuffer;
    int m_ringHead = 0; // 读指针，下一次读取的位置
    int m_ringSize = 0; // 有效数据的总字节数，就是从读指针出发，能访问到的最大逻辑偏移量
};

#endif // PROTOCOLPARSER_H
