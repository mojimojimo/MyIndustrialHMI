#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QLabel>
#include <QSharedPointer>
#include "qcustomplot.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    refreshPorts();
    initChart();
    ui->targetTemp->setDecimals(1);
    ui->targetTemp->setRange(-20.0,100.0);
    ui->btnSetTemp->setEnabled(false); // 默认不可点，直到连接成功


    thread = new QThread;//内存泄漏
    SerialWorker *worker = new SerialWorker;
    ProtocolParser *parser = new ProtocolParser;
    DeviceManager *device = new DeviceManager(this);
    worker->moveToThread(thread);
    parser->moveToThread(thread);

    //开关串口
    connect(this,&MainWindow::signalOpenSerial,worker,&SerialWorker::openSerialPort);
    connect(this,&MainWindow::signalCloseSerial,worker,&SerialWorker::closeSerialPort);

    //发送数据
    connect(this,&MainWindow::signalSendData,device,&DeviceManager::onSendData);
    connect(device,&DeviceManager::sendFrame,parser,&ProtocolParser::buildPacket);
    connect(parser,&ProtocolParser::sendRawData,worker,&SerialWorker::sendData);

    //接收数据
    connect(worker,&SerialWorker::rawDataReceived,parser,&ProtocolParser::onRawDataReceived);
    connect(parser,&ProtocolParser::frameReceived,device,&DeviceManager::onFrameReceived);
    connect(device,&DeviceManager::dataReceived,this,&MainWindow::onDataReceived);

    //分层日志
    //connect(worker,&SerialWorker::logSerial,this,&MainWindow::writeLog);
    connect(worker,&SerialWorker::rawDataReceived,this,[=](QByteArray rawdata){
        if (ui->chkHexDisplay->isChecked()) {    //原始日志 (Hex View)
            QString hexLog = "原始数据: " + rawdata.toHex(' ').toUpper();
            writeLog(hexLog, false);//接收；plainTextEdit默认用 UTF-8 显示文本
        }
    });
    connect(parser,&ProtocolParser::logProtocol,this,&MainWindow::writeLog);
    connect(device,&DeviceManager::logBusiness,this,&MainWindow::writeLog);

    connect(this,&MainWindow::signalDeviceStart,device,&DeviceManager::startDevice);
    connect(device,&DeviceManager::deviceOffline,worker,&SerialWorker::closeSerialPort);

    connect(worker,&SerialWorker::portStatusChanged,this,&MainWindow::onPortStatusChanged);
    connect(worker,&SerialWorker::errorOccuerred,this,[=](QString errorMsg){//指定this
        QMessageBox::critical(this,"错误", errorMsg);
    });


    //线程结束后删除对象
    connect(thread, &QThread::finished, worker, &SerialWorker::deleteLater);
    connect(thread, &QThread::finished, parser, &ProtocolParser::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    //启动线程
    thread->start();
    connect(ui->btnOpen,&QPushButton::clicked,[=](){
        QString port = ui->portList->currentData().toString();//?
        emit signalOpenSerial(port,9600);
    });
    connect(ui->btnClose,&QPushButton::clicked,[=](){
        emit signalCloseSerial();
    });

    connect(ui->btnSetTemp,&QPushButton::clicked,[=](){
        double val = ui->targetTemp->value();
        short sendVal = static_cast<short>(val*10);//定点数传输

        QByteArray data;
        data.append(static_cast<char>(sendVal>>8));//?
        data.append(static_cast<char>(sendVal & 0xFF));
        emit signalSendData(FUNC_SET_PARAM,data);
        QString cleanLog = QString("下发目标温度：%1 ℃").arg(val);//业务日志
        writeLog(cleanLog,true);//C2137
    });

    //加载配置
    QSettings settings("config.ini", QSettings::IniFormat);
    QString lastPort = settings.value("PortName").toString();
    int idx = ui->portList->findText(lastPort);
    if(idx!= -1) ui->portList->setCurrentIndex(idx);

    double lastTemp = settings.value("TargetTemp",0.0).toDouble();
    ui->targetTemp->setValue(lastTemp);

    restoreGeometry(settings.value("Geometry").toByteArray());
}

MainWindow::~MainWindow()
{
    if (thread != nullptr && thread->isRunning()) {
        thread->quit(); // 请求退出事件循环
        thread->wait(); // 主线程等待子线程真正退出
    }
    delete ui;
}

void MainWindow::onPortStatusChanged(bool isOpen){
    if(isOpen){
        ui->lblStatus->setText("已连接");
        ui->lblStatus->setStyleSheet("color: green;");
        ui->btnOpen->setEnabled(false);
        ui->btnClose->setEnabled(true);
        ui->btnSetTemp->setEnabled(true);
        emit signalDeviceStart(true);

    }else{
        ui->lblStatus->setText("未连接");
        ui->lblStatus->setStyleSheet("color: red;");
        ui->btnOpen->setEnabled(true);
        ui->btnClose->setEnabled(false);
        ui->btnSetTemp->setEnabled(false);
        emit signalDeviceStart(false);
    }

}

void MainWindow::onDataReceived(int type, double value){

    if(type==1){
         ui->lblTemp->setText(QString::number(value,'f',1) + " ℃ ");
         QString cleanLog = QString("解析成功：温度 = %1 ℃").arg(value);//业务日志 (Text View)
         writeLog(cleanLog, false);

    }

    // 获取当前时间戳 (秒)
    double key = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
    // 添加数据点
    ui->plotTemp->graph(0)->addData(key, value);
    // 移除太老的数据 (比如只保留最近 100 个点，防止内存爆掉)
    ui->plotTemp->graph(0)->data()->removeBefore(key - 100);
    // 实现自动滚动，最右侧边界key，跨度60s
    ui->plotTemp->xAxis->setRange(key, 60, Qt::AlignRight); // 显示最近60秒
    // 刷新重绘
    ui->plotTemp->replot();
}

void MainWindow::refreshPorts(){
    ui->portList->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for(auto &info:infos){
        QString port = info.portName()+"("+info.description()+")";
        ui->portList->addItem(port,info.portName());//？
    }
}



void MainWindow::writeLog(const QString &text,bool isSend){
    //获取时间戳
    QString timeStr = QDateTime::currentDateTime().toString("[HH:mm:ss.zzz]");
    //收发标志
    QString direction = isSend? "[TX]->":"[RX]<-";
    QString finalLog = timeStr + direction + text;
    ui->txtLog->appendPlainText(finalLog);
}

void MainWindow::initChart(){
    // 1.添加图层
    ui->plotTemp->addGraph();

    // 2.设置画笔颜色
    ui->plotTemp->graph(0)->setPen(QPen(Qt::blue));

    // 3.设置坐标轴标签
    ui->plotTemp->xAxis->setLabel("时间 (s)");
    ui->plotTemp->yAxis->setLabel("温度 (℃)");

    // 4.设置坐标轴范围 (比如温度 0~100)
    ui->plotTemp->yAxis->setRange(0, 50);

    // 5.允许用户拖拽和缩放
    ui->plotTemp->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);//?
    dateTicker->setDateTimeFormat("HH:mm:ss");
    ui->plotTemp->xAxis->setTicker(dateTicker);
    ui->plotTemp->xAxis->setTickLabelRotation(30);

}

void MainWindow::closeEvent(QCloseEvent *event){

    //配置存储：记住用户上次设置
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.setValue("PortName", ui->portList->currentText());
    settings.setValue("TargetTemp", ui->targetTemp->value());
    settings.setValue("Geometry", saveGeometry());
    event->accept(); // 允许窗口关闭

}
