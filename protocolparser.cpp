#include "protocolparser.h"
#include <QDebug>
ProtocolParser::ProtocolParser(QObject *parent)
    : QObject{parent}
{

}

void ProtocolParser::processData(){
    if(m_buffer.size()<6){
        qDebug()<<"半包";
        return;
    }
    while(m_buffer.size()>=6){//当前协议最小包长度为6(无payload)

        //1.帧头校验：移除脏数据
        if(m_buffer.at(0) != static_cast<char> (FRAME_HEAD_1) || m_buffer.at(1) != static_cast<char>(FRAME_HEAD_2)){
            m_buffer.remove(0,1);
            continue;//直到找到帧头
        }
        qDebug()<<"检测到帧头";
        //是否断包,是则等待下一次readyRead
        //修复越界访问：解析串口数据，保证datalen无符号
        unsigned char datalen = static_cast<unsigned char> (m_buffer.at(3));
        //int datalen = static_cast<int> (datalen_uchar);//?
        //2.datalen校验
        if(datalen > PROTOCOL_MAX_DATALEN){
            qDebug()<<"【错误】数据长度非法（超过协议最大值"<<PROTOCOL_MAX_DATALEN<<"）";
            m_buffer.remove(0,2); // 移除非法帧头，避免死循环
            continue;
        }

        //3.包长度校验：断包校验
        int packetSize = 2 + 1 + 1 + datalen + 1 + 1;
        if(m_buffer.size()<packetSize){
            qDebug()<<"断包";
            break;
        }

        //提取整包
        QByteArray packet = m_buffer.left(packetSize);

        //-------------------开始解析整包数据-----------------
        //4.帧尾校验：完整帧格式
        if(packet.at(packetSize-1) != static_cast<char>(FRAME_TAIL)){
            qDebug()<<"【错误包】帧尾校验失败";
            m_buffer.remove(0,1);//防止丢掉一部分真数据
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
           // unsigned char funcCode =
            frame.payload = packet.mid(4, datalen);
            emit frameReceived(frame);

        } else {
            qDebug() << "校验失败！计算值:" << calSum << " 接收值:" << revSum;
        }

        //删除已解析数据或丢弃整包
        qDebug()<<m_buffer.toHex(' ');
        m_buffer.remove(0,packetSize);
        qDebug()<<m_buffer.toHex(' ');
    }
}

void ProtocolParser::buildPacket(Frame frame){

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

void ProtocolParser::onRawDataReceived(QByteArray rawdata){
    //QString logdata = rawdata.toHex(' ').toUpper();
    //if (ui->chkHexDisplay->isChecked()) {    //原始日志 (Hex View)
    //QString hexLog = "原始数据: " + logdata;
    //emit logProtocol(hexLog, false);//接收
    //}
    m_buffer.append(rawdata);
    processData();
}

