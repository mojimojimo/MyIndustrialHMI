#include "protocolparser.h"
#include <QDebug>
#include <QIODevice>
#include <cstring>

ProtocolParser::ProtocolParser(QObject *parent)
    : QObject{parent}
{
    m_ringBuffer.resize(RING_BUFFER_CAPACITY);
    m_ringBuffer.fill('\0');
}

void ProtocolParser::resetRingBuffer()
{
    m_ringHead = 0;
    m_ringSize = 0;
}

int ProtocolParser::ringAvailable() const
{
    return m_ringSize;
}

int ProtocolParser::ringFreeSpace() const
{
    return RING_BUFFER_CAPACITY - m_ringSize;
}

int ProtocolParser::ringTailIndex() const
{
    return (m_ringHead + m_ringSize) % RING_BUFFER_CAPACITY;
}

char ProtocolParser::ringPeek(int offset) const
{
    const int index = (m_ringHead + offset) % RING_BUFFER_CAPACITY;
    return m_ringBuffer.at(index);
}

bool ProtocolParser::ringWrite(const QByteArray &data)
{
    if (data.isEmpty()) {
        return true;
    }

    // 数据长度必须小于等于 ring 缓存总容量，否则无论如何都无法写入完整数据，直接丢弃并报警
    if (data.size() > RING_BUFFER_CAPACITY) {
        return false;
    }

    // 溢出策略: 丢旧保新，尽可能保留最新数据用于实时监控。
    const int needDrop = data.size() - ringFreeSpace();
    if (needDrop > 0) {
        ringDrop(needDrop);
    }

    int tail = ringTailIndex();
    const int firstPart = qMin(data.size(), RING_BUFFER_CAPACITY - tail);
    memcpy(m_ringBuffer.data() + tail, data.constData(), firstPart);

    const int remain = data.size() - firstPart;
    if (remain > 0) {
        memcpy(m_ringBuffer.data(), data.constData() + firstPart, remain);
    }

    m_ringSize += data.size();
    return true;
}

bool ProtocolParser::ringDrop(int count)
{
    if (count < 0 || count > m_ringSize) {
        return false;
    }
    if (count == 0) {
        return true;
    }

    m_ringHead = (m_ringHead + count) % RING_BUFFER_CAPACITY;
    m_ringSize -= count;

    if (m_ringSize == 0) {
        m_ringHead = 0;
    }
    return true;
}

QByteArray ProtocolParser::ringReadSlice(int offset, int length) const
{
    QByteArray result;
    if (offset < 0 || length < 0) {
        return result;
    }
    if (offset + length > m_ringSize) {
        return result;
    }
    if (length == 0) {
        return result;
    }

    result.resize(length);
    const int start = (m_ringHead + offset) % RING_BUFFER_CAPACITY;
    const int firstPart = qMin(length, RING_BUFFER_CAPACITY - start);

    memcpy(result.data(), m_ringBuffer.constData() + start, firstPart);

    const int remain = length - firstPart;
    if (remain > 0) {
        memcpy(result.data() + firstPart, m_ringBuffer.constData(), remain);
    }

    return result;
}

void ProtocolParser::processRawData(){
    if (ringAvailable() < 6) {
        qDebug()<<"半包";
        return;
    }
    while (ringAvailable() >= 6) { // 当前协议最小包长度为6(无payload)

        // 1. 帧头校验: 非法起始字节按1B滑动重同步
        if (ringPeek(0) != static_cast<char>(FRAME_HEAD_1) || ringPeek(1) != static_cast<char>(FRAME_HEAD_2)) {
            ringDrop(1);
            continue;
        }
        qDebug()<<"检测到帧头";
        // 长度字段必须按无符号读取，避免负值造成越界解析
        unsigned char datalen = static_cast<unsigned char>(ringPeek(3));
        // 2. datalen校验
        if (datalen > PROTOCOL_MAX_DATALEN) {
            qDebug()<<"【错误】数据长度非法（超过协议最大值"<<PROTOCOL_MAX_DATALEN<<"）";
            emit logProtocol("DEBUG", "数据长度非法（超过协议最大值）");
            ringDrop(2);
            continue;
        }

        // 3. 包长度校验: 半包时等待下一次输入
        int packetSize = 2 + 1 + 1 + datalen + 1 + 1;
        if (ringAvailable() < packetSize) {
            qDebug()<<"断包";
            emit logProtocol("DEBUG", "断包");
            break;
        }

        // 4~5步基于相对head偏移读取，避免复制整包
        // 4. 帧尾校验
        if (ringPeek(packetSize - 1) != static_cast<char>(FRAME_TAIL)) {
            qDebug()<<"【错误包】帧尾校验失败";
            emit logProtocol("DEBUG", "帧尾校验失败");
            ringDrop(1);
            continue;
        }
        // 5. 校验码校验
        unsigned char calSum = 0;
        for (int i = 2; i < packetSize - 2; ++i) {
            calSum += static_cast<unsigned char>(ringPeek(i));
        }
        unsigned char revSum = static_cast<unsigned char>(ringPeek(packetSize - 2));
        if (calSum == revSum) {
            Frame frame;
            frame.funcCode = static_cast<quint8>(ringPeek(2));
            frame.payload = ringReadSlice(4, datalen);
            processFrame(frame);

        } else {
            qDebug() << "校验失败！计算值:" << calSum << " 接收值:" << revSum;
            emit logProtocol("DEBUG", "校验码不符");
        }

        ringDrop(packetSize);
    }
}

void ProtocolParser::processFrame(const Frame &frame) {
    if (frame.funcCode == FUNC_REPORT_ALL_DATA) {
        if (frame.payload.size() < 7) return; // 长度安全校验

        DeviceData data;

        // 解析温度(2B) 大端
        uint8_t high = static_cast<uint8_t> (frame.payload.at(0));
        uint8_t low  = static_cast<uint8_t> (frame.payload.at(1));
        int16_t rawTemp = (high<<8) | low;
        double temp = rawTemp / 10.0;

        //DatabaseManager::instance().insertData(temp);
        //emit dataReceived(1,temp);

        // 解析湿度(2B)
        uint16_t rawHum = (static_cast<uint8_t> (frame.payload.at(2))<<8) |
                           static_cast<uint8_t> (frame.payload.at(3));
        double hum = rawHum / 10.0;

        // 解析状态(各1B)
        data.actualTemperature = temp;
        data.actualHumidity    = hum;
        data.doorStatus        = static_cast<uint8_t> (frame.payload.at(4));
        data.compressorStatus  = static_cast<uint8_t> (frame.payload.at(5));
        data.alarmCode         = static_cast<uint8_t> (frame.payload.at(6));

        qDebug() << "解析成功! 温度:" << data.actualTemperature
                 << "湿度:" << data.actualHumidity
                 << "门:" << data.doorStatus
                 << "压缩机:" << data.compressorStatus
                 <<"故障码:"<< data.alarmCode;

        emit RealtimeDataParsed(data);
    }
    else if (frame.funcCode == FUNC_PARAM_RETURN) {
        if(frame.payload.size() < 12) return;

        QDataStream stream(frame.payload);
        qint16 tTarget, tHigh, tLow;
        quint16 hTarget, hHigh, hLow;

        stream >> tTarget >> tHigh >> tLow >> hTarget >> hHigh >> hLow;

        ConfigData config;
        config.targetTemperature = tTarget / 10.0;
        config.tempHighLimit     = tHigh / 10.0;
        config.tempLowLimit      = tLow / 10.0;
        config.targetHumidity    = hTarget / 10.0;
        config.humidHighLimit    = hHigh / 10.0;
        config.humidLowLimit     = hLow / 10.0;

        emit configParamLoaded(config);
    }
    else if (frame.funcCode == FUNC_CMD_ACK) {
        if(frame.payload.isEmpty()) return;
        quint8 result = static_cast<quint8> (frame.payload.at(0));
        bool ack = (result == 0x00);

        emit cmdAckReceived(ack, result);
    } else {
        qDebug()<<"功能码错误！";
    }
}

void ProtocolParser::onPackReadParam(){
    Frame frame;
    frame.funcCode = FUNC_READ_PARAM;
    frame.payload = QByteArray();
    buildPacket(frame);
}

void ProtocolParser::onPackWriteParam(const ConfigData &config){

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);

    stream << static_cast<qint16>(config.targetTemperature * 10.0);
    stream << static_cast<qint16>(config.tempHighLimit * 10.0);
    stream << static_cast<qint16>(config.tempLowLimit * 10.0);
    stream << static_cast<quint16>(config.targetHumidity * 10.0);
    stream << static_cast<quint16>(config.humidHighLimit * 10.0);
    stream << static_cast<quint16>(config.humidLowLimit * 10.0);

    Frame frame;
    frame.funcCode = FUNC_WRITE_PARAM;
    frame.payload = payload;
    buildPacket(frame);
}

void ProtocolParser::onPackCmd(const QString &cmd){
    Frame frame;
    frame.funcCode = FUNC_CTRL_CMD;
    frame.payload = QByteArray::fromHex(cmd.toUtf8());
    buildPacket(frame);
}

void ProtocolParser::buildPacket(const Frame &frame){ // 应用层封包

    QByteArray packet;
    packet.append(static_cast<char>(FRAME_HEAD_1));//帧头
    packet.append(static_cast<char>(FRAME_HEAD_2));

    packet.append(frame.funcCode); // 功能码
    packet.append(static_cast<char>(frame.payload.size())); // 数据长度(1B)

    packet.append(frame.payload);

    // 计算校验和: 从功能码开始累加到payload末尾
    unsigned char sum =0;
    for(int i=2;i<packet.size();i++){
        sum+= static_cast<unsigned char>(packet.at(i));
    }
    packet.append(sum);
    packet.append(static_cast<char>(FRAME_TAIL));
    emit sendRawData(packet);
    emit logProtocol("DEBUG", "[TX] " + packet.toHex(' ').toUpper());
}

void ProtocolParser::onRawDataReceived(const QByteArray &rawdata){
    // 剩余空间不足时丢弃旧数据以保留新数据，避免监控数据滞后过久
    const int freeBefore = ringFreeSpace();
    const int droppedOldBytes = (rawdata.size() <= RING_BUFFER_CAPACITY && rawdata.size() > freeBefore)
                                    ? (rawdata.size() - freeBefore)
                                    : 0;
                                    
    // 当输入数据超过总容量时直接丢弃并报警，避免死循环;否则丢弃部分数据
    if (!ringWrite(rawdata)) {
        emit logProtocol("WARNING", QString("ring buffer空间不足，本次输入已丢弃(size=%1, cap=%2)")
                                       .arg(rawdata.size())
                                       .arg(RING_BUFFER_CAPACITY));
    } else if (droppedOldBytes > 0) {
        emit logProtocol("WARNING", QString("ring buffer溢出，已丢弃旧数据%1B以保留新数据")
                                       .arg(droppedOldBytes));
    }
    processRawData();
    emit logProtocol("DEBUG", "[RX] " + rawdata.toHex(' ').toUpper());
}

