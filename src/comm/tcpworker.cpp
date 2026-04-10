#include "tcpworker.h"
#include <QDebug>

TcpWorker::TcpWorker(CommWorker *parent)
    : CommWorker{parent}
{
    socket = new QTcpSocket(this);

    connect(socket,&QTcpSocket::readyRead,this,&TcpWorker::onReadyRead);

    connect(socket,&QTcpSocket::connected,this,[=](){
        emit logComm("INFO", "TCP连接成功");
        emit StatusChanged(true);
    });
    connect(socket,&QTcpSocket::disconnected,this,[=](){
        emit logComm("INFO", "TCP连接断开");
        emit StatusChanged(false);
    });

    connect(socket,&QTcpSocket::errorOccurred,this,[=](QAbstractSocket::SocketError){
        emit logComm("ERROR", socket->errorString());
    });
}

TcpWorker::~TcpWorker(){
    socket->close();
}

void TcpWorker::open(QString target,int portOrBaud){

    if(socket->state() == QAbstractSocket::ConnectedState) return;
    socket->abort(); //终止之前的连接
    socket->connectToHost(target,portOrBaud);
}

void TcpWorker::close(){
    socket->disconnectFromHost();//发完缓冲区里的数据才关闭
}

void TcpWorker::sendData(const QByteArray &data){
    socket->write(data);
    qDebug()<<"tcp"<<data.toHex(' ').toUpper();
    socket->flush();
}

void TcpWorker::onReadyRead(){
    QByteArray data = socket->readAll();
    qDebug()<<"tcp"<<data.toHex(' ').toUpper();
    emit rawDataReceived(data);
}
