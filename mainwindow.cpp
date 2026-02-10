#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    refreshPorts();

    thread = new QThread;//内存泄漏
    SerialWorker *worker = new SerialWorker;
    worker->moveToThread(thread);
    //qDebug()<<"sjsjd";

    connect(this,&MainWindow::signalOpenSerial,worker,&SerialWorker::openSerialPort);
    connect(this,&MainWindow::signalCloseSerial,worker,&SerialWorker::closeSerialPort);
    connect(this,&MainWindow::signalSendData,worker,&SerialWorker::sendData);

    connect(worker,&SerialWorker::portStatusChanged,this,&MainWindow::onPortStatusChanged);
    connect(worker,&SerialWorker::dataReceived,this,&MainWindow::onDataReceived);
    connect(worker,&SerialWorker::errorOccuerred,this,[=](QString errorMsg){//我靠要指定this?
        QMessageBox::critical(this,"警告","串口打开失败" + errorMsg);
    });
    connect(worker,&SerialWorker::rawDataReceived,this,[=](QString s){
        writeLog(s,false);//接收
    });


    // // 当线程结束运行时，自动删除 worker 对象
    // connect(thread, &QThread::finished, worker, &SerialWorker::deleteLater);
    // // 当线程结束运行时，自动删除 thread 对象自己
    // connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    //启动线程
    thread->start();
    connect(ui->btnOpen,&QPushButton::clicked,[=](){
        QString port = ui->portList->currentData().toString();//?
        emit signalOpenSerial(port,9600);
    });
    connect(ui->btnClose,&QPushButton::clicked,[=](){
        emit signalCloseSerial();
    });
    QStringList();

    timer = new QTimer(this);
    connect(timer,&QTimer::timeout,[=](){
        if(ui->btnClose->isEnabled()){//？
            //QByteArray queryCmd = QByteArray::fromHex("AA 55 03 00 03 FF");
            QByteArray queryCmd;
            QByteArray finalPacket = buildPacket(0x03,queryCmd);//传的参是funccode+data
            emit signalSendData(finalPacket);
            // 发送日志
            writeLog(finalPacket.toHex(' ').toUpper(), true); // true表示发送
        }
    });
    //timer->start(1000);
}

MainWindow::~MainWindow()
{
    // 1. 告诉线程“你可以下班了”
    // if (thread != nullptr && thread->isRunning()) {
    //     thread->quit(); // 请求退出事件循环
    //     thread->wait(); // 主线程在这里死等，直到子线程真正退出（防止主线程先死，子线程变成孤儿）
    // }
    delete ui;
}

void MainWindow::onPortStatusChanged(bool isOpen){
    if(isOpen){
        ui->lblStatus->setText("已连接");
        ui->lblStatus->setStyleSheet("color: green;");
        ui->btnOpen->setEnabled(false);
        ui->btnClose->setEnabled(true);
        timer->start(1000);
    }else{
        ui->lblStatus->setText("未连接");
        ui->lblStatus->setStyleSheet("color: red;");
        ui->btnOpen->setEnabled(true);
        ui->btnClose->setEnabled(false);
        timer->stop();
    }
}

void MainWindow::onDataReceived(int type, double value){
    if(type==1){
         ui->lblTemp->setText(QString::number(value,'f',1) + " ℃ ");
    }
    //ui->logTextEdit->setText(logStr);
}

void MainWindow::refreshPorts(){
    ui->portList->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for(auto &info:infos){
        QString port = info.portName()+"("+info.description()+")";
        ui->portList->addItem(port,info.portName());//？
    }
}

QByteArray MainWindow::buildPacket(char funcCode, const QByteArray &dataContent){
    QByteArray packet;
    packet.append(static_cast<char>(0xAA));//帧头
    packet.append(static_cast<char>(0x55));

    packet.append(funcCode);//功能码 int->Hex
    packet.append(static_cast<char>(dataContent.size()));//数据长度 qsizetype怎么转16进制

    packet.append(dataContent);

    //计算校验和
    unsigned char sum =0;
    sum+=(unsigned char)funcCode;
    sum+=(unsigned char)dataContent.size();
    for(char c :packet){
        sum+=(unsigned char)c;//ascII?
    }
    packet.append(sum);
    packet.append(static_cast<char>(0xFF));
    return packet;
}

void MainWindow::writeLog(const QString &text,bool isSend){
    //获取时间戳
    QString timeStr = QDateTime::currentDateTime().toString("[HH:mm:ss.zzz]");
    //收发标志
    QString direction = isSend? "[TX]->":"[RX]<-";
    QString finalLog = timeStr + direction + text;
    ui->txtLog->appendPlainText(finalLog);
}

