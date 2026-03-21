#include "protocolparser.h"
#include <QDebug>
#include <QIODevice>
ProtocolParser::ProtocolParser(QObject *parent)
    : QObject{parent}
{

}

void ProtocolParser::processRawData(){
    if(m_buffer.size() - m_readIndex<6){
        qDebug()<<"半包";
        return;
    }
    while(m_buffer.size() - m_readIndex>=6){//当前协议最小包长度为6(无payload)

        //1.帧头校验：移除脏数据
        if(m_buffer.at(m_readIndex) != static_cast<char> (FRAME_HEAD_1) || m_buffer.at(m_readIndex+1) != static_cast<char>(FRAME_HEAD_2)){
           // m_buffer.remove(0,1);
            m_readIndex++;
            continue;//直到找到帧头
        }
        qDebug()<<"检测到帧头";
        //是否断包,是则等待下一次readyRead
        //修复越界访问：解析串口数据，保证datalen无符号
        unsigned char datalen = static_cast<unsigned char> (m_buffer.at(3+m_readIndex));
        //int datalen = static_cast<int> (datalen_uchar);//?
        //2.datalen校验
        if(datalen > PROTOCOL_MAX_DATALEN){
            qDebug()<<"【错误】数据长度非法（超过协议最大值"<<PROTOCOL_MAX_DATALEN<<"）";
            //m_buffer.remove(0,2); // 移除非法帧头，避免死循环
            m_readIndex+=2;
            continue;
        }

        //3.包长度校验：断包校验
        int packetSize = 2 + 1 + 1 + datalen + 1 + 1;
        if(m_buffer.size()-m_readIndex<packetSize){
            qDebug()<<"断包";
            break;
        }

        //提取整包
        //QByteArray packet = m_buffer.left(packetSize);
        QByteArray packet = m_buffer.mid(m_readIndex,packetSize);

        //-------------------开始解析整包数据-----------------
        //4.帧尾校验：完整帧格式
        if(packet.at(packetSize-1) != static_cast<char>(FRAME_TAIL)){
            qDebug()<<"【错误包】帧尾校验失败";
            //m_buffer.remove(0,1);//防止丢掉一部分真数据
            m_readIndex++;
            continue;
        }
        //5.校验码：校验payload
        unsigned char calSum = 0;
        for(int i=2;i<packetSize-2;++i){
            calSum += static_cast<unsigned char> (packet.at(i));
        }
        //calSum = calSum & 0xFF;
        unsigned char revSum = static_cast<unsigned char>(packet.at(packetSize-2));
        if(calSum == revSum){
            Frame frame;
            frame.funcCode = static_cast<quint8> (packet.at(2));
            frame.payload = packet.mid(4, datalen);
            //emit frameReceived(frame);
            processFrame(frame);

        } else {
            qDebug() << "校验失败！计算值:" << calSum << " 接收值:" << revSum;
        }

        //删除已解析数据或丢弃整包
        // qDebug()<<m_buffer.toHex(' ');
        // m_buffer.remove(0,packetSize);
        // qDebug()<<m_buffer.toHex(' ');
        qDebug()<<m_buffer.right(m_buffer.size()-m_readIndex).toHex(' ');
        m_readIndex+=packetSize;
        qDebug()<<m_buffer.right(m_buffer.size()-m_readIndex).toHex(' ');
    }
    //批量清理垃圾数据
    if(m_readIndex > 2048 || m_readIndex>m_buffer.size()/2){
        m_buffer.remove(0,m_readIndex);
        m_readIndex=0;
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
    else if (frame.funcCode == FUNC_PARAM_RETURN) {//
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

void ProtocolParser::onPackCmd(){
    Frame frame;
    frame.funcCode = FUNC_CTRL_CMD;
    frame.payload = QByteArray::fromHex("01");//
    buildPacket(frame);
}

void ProtocolParser::buildPacket(const Frame &frame){//应用层封包

    QByteArray packet;
    packet.append(static_cast<char>(FRAME_HEAD_1));//帧头
    packet.append(static_cast<char>(FRAME_HEAD_2));

    packet.append(frame.funcCode);//功能码 int->Hex
    packet.append(static_cast<char>(frame.payload.size()));//数据长度 qsizetype怎么转16进制

    packet.append(frame.payload);

    //计算校验和
    unsigned char sum =0;
    //sum计算修正
    for(int i=2;i<packet.size();i++){
        sum+= static_cast<unsigned char>(packet.at(i));
        //qDebug()<<"校验和3"<<sum;
    }
    //qDebug()<<"校验和"<<sum;
    //sum = sum & 0xFF;
    packet.append(sum);
    packet.append(static_cast<char>(FRAME_TAIL));
    emit sendRawData(packet);
    emit logProtocol(packet.toHex(' ').toUpper(),true);
}

void ProtocolParser::onRawDataReceived(const QByteArray &rawdata){
    m_buffer.append(rawdata);
    processRawData();
}

