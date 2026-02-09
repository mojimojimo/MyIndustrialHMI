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

    m_buffer.append(serial->readAll());//追加数据
    processData();
}

void SerialWorker::openSerialPort(QString portName,int baudRate){
    qDebug()<<"222";

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
    if(m_buffer.size()<7){
        qDebug()<<"banbao";
    }
    while(m_buffer.size()>=7){//当前协议最小包长度为7

        //检测帧头，移除脏数据
        if(m_buffer.at(0) != static_cast<char> (0xAA) || m_buffer.at(1) != static_cast<char>(0x55)){
            m_buffer.remove(0,1);
            continue;//直到找到帧头
        }
        qDebug()<<"检测到帧头";
        //是否断包,是则等待下一次readyRead
        int datalen = static_cast<int> (m_buffer.at(3));
        int packetSize = 2+1+1+datalen+1+1;
        if(m_buffer.size()<packetSize){
            qDebug()<<"banbao7";
            break;
        }

        //提取整包
        QByteArray packet = m_buffer.left(packetSize);

        //-------------------开始解析数据-----------------
        //帧尾
        if(packet.at(packetSize-1)==static_cast<char>(0xFF)){
            unsigned char funcCode =(unsigned char) packet.at(2);//?
            QByteArray content = packet.mid(4, datalen);

            if(funcCode == 0x01){//？
                //1B 温度值
                int temp =(unsigned char) content.at(0);
                emit dataReceived(1,temp);//1?
                qDebug()<<"温度:"<<temp<<"oC";
            }else if(funcCode == 0x02){
                qDebug()<<"电机转速:";
            }
        }else{
            qDebug()<<"【错误包】帧尾校验失败";
        }

        //删除已解析数据
        qDebug()<<m_buffer.toHex(' ');
        m_buffer.remove(0,packetSize);
        qDebug()<<m_buffer.toHex(' ');
    }
}

