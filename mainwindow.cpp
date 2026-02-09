#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "serialworker.h"
#include <QMessageBox>
#include <QThread>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QThread *thread = new QThread(this);//内存泄漏
    SerialWorker *worker = new SerialWorker;
    worker->moveToThread(thread);
    //qDebug()<<"sjsjd";

    connect(this,&MainWindow::signalOpenSerial,worker,&SerialWorker::openSerialPort);
    connect(this,&MainWindow::signalCloseSerial,worker,&SerialWorker::closeSerialPort);

    connect(worker,&SerialWorker::portStatusChanged,this,&MainWindow::onPortStatusChanged);
    connect(worker,&SerialWorker::dataReceived,this,&MainWindow::onDataReceived);
    // connect(worker,&SerialWorker::errorOccuerred,[=](QString errorMsg){
    //     QMessageBox::warning(this,"警告",errorMsg);
    // });

    //启动线程
    thread->start();
    connect(ui->btnOpen,&QPushButton::clicked,[=](){
        QString port = ui->portList->currentText();
        emit signalOpenSerial(port,9600);
    });
    connect(ui->btnClose,&QPushButton::clicked,[=](){
        emit signalCloseSerial();
    });

    ui->portList->addItem("COM1");
    ui->portList->addItem("COM2");
}

MainWindow::~MainWindow()
{

    delete ui;
}

void MainWindow::onPortStatusChanged(bool isOpen){
    if(isOpen){
        ui->lblStatus->setText("已连接");
        ui->lblStatus->setStyleSheet("color: green;");
        ui->btnOpen->setEnabled(false);
        ui->btnClose->setEnabled(true);
    }else{
        ui->lblStatus->setText("未连接");
        ui->lblStatus->setStyleSheet("color: red;");
        ui->btnOpen->setEnabled(true);
        ui->btnClose->setEnabled(false);
    }
}

void MainWindow::onDataReceived(int type, double value){
    if(type==1){
         ui->lblTemp->setText(QString::number(value,'f',1) + " ℃ ");
    }

}

