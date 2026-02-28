#include "tcpworker.h"
#include <QDebug>

TcpWorker::TcpWorker(CommWorker *parent)
    : CommWorker{parent}
{
    socket = new QTcpSocket(this);

    connect(socket,&QTcpSocket::readyRead,this,&TcpWorker::onReadyRead);

    connect(socket,&QTcpSocket::connected,this,[=](){//它的正常断开是会自动发这个connected/disconnected信号，但异常断开不会，还需要应用层发心跳包超时响应来确定
        qDebug()<<"tcp连接成功";//QAbstractSocket::SocketError err？啥写法
        emit StatusChanged(true);//this?
    });
    connect(socket,&QTcpSocket::disconnected,[=](){
        qDebug()<<"tcp连接断开";
        emit StatusChanged(false);
    });

    //错误处理 传入枚举参数？和普通的形如int a参数怎么对应的？
    connect(socket,&QTcpSocket::errorOccurred,this,[=](QAbstractSocket::SocketError){
        qDebug()<<"aaa";
        emit errorOccurred(socket->errorString());

    });
}

TcpWorker::~TcpWorker(){
    socket->close();
}

void TcpWorker::open(QString target,int portOrBaud){

    if(socket->state() == QAbstractSocket::ConnectedState) return;
    socket->abort(); //终止之前的连接

    socket->connectToHost(target,portOrBaud);

    //socket->waitForConnected(3000); // 阻塞等待3秒，或者改成非阻塞也可以

}

void TcpWorker::close(){
    //socket->close();为啥这儿不像串口那样直接close?
    socket->disconnectFromHost();//发完缓冲区里的数据才关闭
}

void TcpWorker::sendData(const QByteArray &data){
    socket->write(data);
    qDebug()<<"tcp"<<data;//"\xAAU\x03\x00\x03\xFF"
    socket->flush();
}

void TcpWorker::onReadyRead(){
    QByteArray data = socket->readAll();
    qDebug()<<"tcp"<<data.toHex();//"0011ffaa55aa55010200fafdff6609ff"
    emit rawDataReceived(data);//readAll 只能调一次
}
