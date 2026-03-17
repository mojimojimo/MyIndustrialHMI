#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "historydialog.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QLabel>
#include <QSharedPointer>
#include "qcustomplot.h"
#include <QDebug>
#include <QComboBox>

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

    ui->lcdTemp->setMode(QLCDNumber::Dec);

    connect(ui->modeList,&QComboBox::currentTextChanged,[=](QString text){
        if(text == "串口通信"){
            ui->stackedWidget->setCurrentIndex(0);
        }else{
            ui->stackedWidget->setCurrentIndex(1);
        }
    });


    DeviceManager *device = new DeviceManager(this);

    //发送数据
    connect(this,&MainWindow::signalSendData,device,&DeviceManager::onSendData);
    //接收数据
    connect(device,&DeviceManager::dataReceived,this,&MainWindow::onDataReceived);

    connect(device,&DeviceManager::statusChanged,this,&MainWindow::onStatusChanged);

    //分层日志
    connect(device,&DeviceManager::logBusiness,this,&MainWindow::writeLog);

    connect(device,&DeviceManager::errorOccurred,this,[=](QString errorMsg){//指定this
        QMessageBox::critical(this,"错误", errorMsg);
    });


    connect(ui->btnOpen,&QPushButton::clicked,[=](){
        int mode = ui->modeList->currentIndex();//
        QString target;
        int portOrBaud;
        if(mode){//mode和mode==0
            target = ui->ipEdit->text();//mode==1:TCP
            portOrBaud = ui->portEdit->text().toInt();
        }else{
            target = ui->portList->currentData().toString();
            portOrBaud = ui->baudList->currentText().toInt();
        }
        //qDebug()<<target<<mode<<portOrBaud;
        device->requestOpen(mode,target,portOrBaud);
    });

    connect(ui->btnClose,&QPushButton::clicked,[=](){
        device->requestClose();
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

    connect(ui->btnHistory,&QPushButton::clicked,[=](){
        HistoryDialog dlg(this);
        dlg.exec(); // 模态显示
    });

    //加载配置
    QSettings settings("config.ini", QSettings::IniFormat);
    QString lastPort = settings.value("PortName").toString();
    QString lastBaud = settings.value("Baud").toString();
    QString lastIp = settings.value("IP").toString();
    QString port = settings.value("Port").toString();

    int idx1 = ui->portList->findText(lastPort);
    if(idx1!= -1) ui->portList->setCurrentIndex(idx1);
    int idx2 = ui->baudList->findText(lastBaud);
    if(idx2!= -1) ui->baudList->setCurrentIndex(idx2);
    ui->ipEdit->setText(lastIp);
    ui->portEdit->setText(port);

    double lastTemp = settings.value("TargetTemp",0.0).toDouble();
    ui->targetTemp->setValue(lastTemp);

    restoreGeometry(settings.value("Geometry").toByteArray());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onStatusChanged(bool isOpen){
    if(isOpen){
        ui->lblStatus->setText("已连接");
        ui->lblStatus->setStyleSheet("color: green;");
        ui->lblLight->setStyleSheet(R"(QLabel {
            min-width: 16px;
            min-height: 16px;
            max-width: 16px;
            max-height: 16px;
            background-color: green;
            border-radius: 8px;
            border: 1px solid #666;
        })");

        ui->btnOpen->setEnabled(false);
        ui->btnClose->setEnabled(true);
        ui->btnSetTemp->setEnabled(true);
        //emit signalDeviceStart(true);

    }else{
        ui->lblStatus->setText("未连接");
        ui->lblStatus->setStyleSheet("color: red;");
        ui->lblLight->setStyleSheet(R"(QLabel {
            min-width: 16px;
            min-height: 16px;
            max-width: 16px;
            max-height: 16px;
            background-color: red;
            border-radius: 8px;
            border: 1px solid #666;
        })");
        ui->btnOpen->setEnabled(true);
        ui->btnClose->setEnabled(false);
        ui->btnSetTemp->setEnabled(false);
        //emit signalDeviceStart(false);
    }

}

void MainWindow::onDataReceived(const DeviceData &data){

    qDebug()<<"wolail";
        // ui->lblTemp->setText(QString::number(value,'f',1) + " ℃ ");
         ui->lcdTemp->display(QString::number(data.actualTemperature,'f',1));
         QString cleanLog = QString("解析成功：温度 = %1 ℃").arg(data.actualTemperature);//业务日志 (Text View)
         writeLog(cleanLog, false);

    // 获取当前时间戳 (秒)
    double key = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
    // 添加数据点
    ui->plotTemp->graph(0)->addData(key, data.actualTemperature);
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

    ui->baudList->clear();

    ui->modeList->addItem("串口通信");
    ui->modeList->addItem("TCP客户端");
    QStringList items;
    items << "9600" << "14400" << "19200" <<"115200";
    ui->baudList->addItems(items);

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
    settings.setValue("Baud",ui->baudList->currentText());
    settings.setValue("IP", ui->ipEdit->text());
    settings.setValue("Port",ui->portEdit->text());

    settings.setValue("TargetTemp", ui->targetTemp->value());
    settings.setValue("Geometry", saveGeometry());
    event->accept(); // 允许窗口关闭

}
