#include "serialworker.h"
#include<QDebug>

SerialWorker::SerialWorker(QObject *parent)
    : QObject{parent}
{
    serial = new QSerialPort(this);

    //串口接收缓冲区有数据
    connect(serial,&QSerialPort::readyRead,this,&SerialWorker::onReadyRead);
    //qDebug()<<"111";
}

SerialWorker::~SerialWorker(){
    if(serial->isOpen()){
        serial->close();
    }
}

void SerialWorker::onReadyRead(){
    QByteArray data = serial->readAll();
    m_buffer.append(data);//追加数据
    emit rawDataReceived(data.toHex(' '));
    processData();
}

void SerialWorker::openSerialPort(QString portName,int baudRate){
    //qDebug()<<"222";

    if (serial->isOpen()) serial->close();//?

    //配置串口参数
    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    //8N1
    // serial->setDataBits(QSerialPort::Data8);
    // serial->setParity(QSerialPort::NoParity);
    // serial->setStopBits(QSerialPort::OneStop);

    //打开串口
    if(serial->open(QIODevice::ReadWrite)){//openmode?
        qDebug() << "串口打开成功：COM1";
        emit portStatusChanged(true);
    } else {
        emit errorOccuerred(serial->errorString());
        emit portStatusChanged(false);
        qDebug() << "串口打开失败："<<serial->errorString();

    }
}

void SerialWorker::closeSerialPort(){
    serial->close();
    emit portStatusChanged(false);
}

void SerialWorker::sendData(QByteArray data){
    //发数据 qint64
    //QByteArray sendData = QByteArray::fromHex("AA55020119FF");//AA 55 02 01 19 3F
    if (serial && serial->isOpen()) {
        serial->write(data);
    }
}

void SerialWorker::processData(){
    if(m_buffer.size()<6){
        qDebug()<<"banbao";
        return;
    }
    while(m_buffer.size()>=6){//当前协议最小包长度为6(无payload)

        //1.帧头校验：移除脏数据
        if(m_buffer.at(0) != static_cast<char> (0xAA) || m_buffer.at(1) != static_cast<char>(0x55)){
            m_buffer.remove(0,1);
            continue;//直到找到帧头
        }
        qDebug()<<"检测到帧头";
        //是否断包,是则等待下一次readyRead
        //修复越界访问：解析串口数据，保证datalen无符号
        unsigned char datalen = static_cast<unsigned char> (m_buffer.at(3));
        //int datalen = static_cast<int> (datalen_uchar);//?
        //2.datalen校验
        // if(datalen > PROTOCOL_MAX_DATALEN){
        //     qDebug()<<"【错误】数据长度非法（超过协议最大值"<<PROTOCOL_MAX_DATALEN<<"）";
        //     m_buffer.remove(0,2); // 移除非法帧头，避免死循环
        //     continue;
        // }

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
        if(packet.at(packetSize-1) != static_cast<char>(0xFF)){
            qDebug()<<"【错误包】帧尾校验失败";
            m_buffer.remove(0,1);//防止丢掉一部分真数据
            continue;
        }
        //5.校验码：校验payload
        unsigned char calSum = 0;
        for(int i=2;i<packetSize-2;++i){
            calSum += static_cast<unsigned char> (packet.at(i));
        }
        unsigned char revSum = static_cast<unsigned char>(packet.at(packetSize-2));
        if(calSum == revSum){
            unsigned char funcCode = static_cast<unsigned char> (packet.at(2));
            QByteArray content = packet.mid(4, datalen);
            if(funcCode == 0x01){//整型提升
                if(content.size()>0){
                    //1B 温度值
                    double temp =static_cast<unsigned char>( content.at(0));//?-3°C?
                    emit dataReceived(1,temp);
                    qDebug()<<"温度:"<<temp<<" ℃ ";
                }
            }else if(funcCode == 0x02){
                qDebug()<<"电机转速:";
            }
        } else {
            qDebug() << "校验失败！计算值:" << calSum << " 接收值:" << revSum;
        }

        //删除已解析数据或丢弃整包
        qDebug()<<m_buffer.toHex(' ');
        m_buffer.remove(0,packetSize);
        qDebug()<<m_buffer.toHex(' ');
    }
}

