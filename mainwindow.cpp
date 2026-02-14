#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
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

    connect(ui->btnSetTemp,&QPushButton::clicked,[=](){
        double val = ui->targetTemp->value();
        short sendVal = static_cast<short>(val*10);//定点数传输

        QByteArray data;
        data.append(static_cast<char>(sendVal>>8));//?
        data.append(static_cast<char>(sendVal & 0xFF));
        QByteArray packet = buildPacket(FUNC_SET_PARAM,data);
        emit signalSendData(packet);
        writeLog("下发目标温度："+packet.toHex(' ').toUpper(),true);//C2137
    });
    thread = new QThread;//内存泄漏
    SerialWorker *worker = new SerialWorker;
    worker->moveToThread(thread);

    connect(this,&MainWindow::signalOpenSerial,worker,&SerialWorker::openSerialPort);
    connect(this,&MainWindow::signalCloseSerial,worker,&SerialWorker::closeSerialPort);
    connect(this,&MainWindow::signalSendData,worker,&SerialWorker::sendData);

    connect(worker,&SerialWorker::portStatusChanged,this,&MainWindow::onPortStatusChanged);
    connect(worker,&SerialWorker::dataReceived,this,&MainWindow::onDataReceived);
    connect(worker,&SerialWorker::errorOccuerred,this,[=](QString errorMsg){//我靠要指定this?
        QMessageBox::critical(this,"错误", errorMsg);
    });
    connect(worker,&SerialWorker::rawDataReceived,this,[=](QString rawdata){
        if (ui->chkHexDisplay->isChecked()) {    //原始日志 (Hex View)
            QString hexLog = "原始数据: " + rawdata;
            writeLog(hexLog, false);//接收
        }
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

    timer = new QTimer(this);
    timeoutTimer = new QTimer(this);

    connect(timer,&QTimer::timeout,[=](){
        if(ui->btnClose->isEnabled()){//？
            //QByteArray queryCmd = QByteArray::fromHex("AA 55 03 00 03 FF");
            QByteArray queryCmd;
            QByteArray finalPacket = buildPacket(FUNC_READ_TEMP,queryCmd);//传的参是funccode+data
            emit signalSendData(finalPacket);

            // 发送日志
            writeLog(finalPacket.toHex(' ').toUpper(), true); // true表示发送
        }
    });
    //timer->start(1000);
    connect(timeoutTimer,&QTimer::timeout,[=](){
        if(!responseTimer.isValid()) return;
        if(responseTimer.elapsed()>2000){
            qDebug() << "检测到超时！准备断开...";
            //顺序
            timeoutTimer->stop();
            timer->stop();
            emit signalCloseSerial();
            QString errorMsg ="设备已下线！（超时未响应）";
            QMessageBox::critical(this,"错误", errorMsg);
            //emit errorOccuerred(errorMsg);//只能发本类的信号
            responseTimer.invalidate();
        }
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
        ui->btnSetTemp->setEnabled(true);
        timer->start(1000);
        timeoutTimer->start(500);
        // 【新增】连接刚建立时，重置一次计时器，给设备2秒的反应时间
        responseTimer.start();
    }else{
        ui->lblStatus->setText("未连接");
        ui->lblStatus->setStyleSheet("color: red;");
        ui->btnOpen->setEnabled(true);
        ui->btnClose->setEnabled(false);
        ui->btnSetTemp->setEnabled(false);
        timer->stop();
        timeoutTimer->stop();
        responseTimer.invalidate();
    }
}

void MainWindow::onDataReceived(int type, double value){
    responseTimer.restart();

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

QByteArray MainWindow::buildPacket(char funcCode, const QByteArray &dataContent){
    QByteArray packet;
    packet.append(static_cast<char>(FRAME_HEAD_1));//帧头
    packet.append(static_cast<char>(FRAME_HEAD_2));

    packet.append(funcCode);//功能码 int->Hex
    packet.append(static_cast<char>(dataContent.size()));//数据长度 qsizetype怎么转16进制

    packet.append(dataContent);

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
