#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "historywidget.h"
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
    setWindowTitle("🔎 疫苗冷藏箱监控HMI系统");
    refreshPorts();
    initChart();
    ui->targetTemp->setDecimals(1);
    ui->targetTemp->setRange(-20.0,100.0);
    ui->btnSetTemp->setEnabled(false); // 默认不可点，直到连接成功

    //全局状态栏
    lblCommStatus = new QLabel("⚫ 未连接", this);//
    lblGlobalAlarm = new QLabel("", this);
    QLabel *lblTime = new QLabel("当前时间：00:00:00", this);

    // 样式设置
    lblGlobalAlarm->setStyleSheet("color: red; font-weight: bold;");//

    // 添加到状态栏 (addPermanentWidget 会靠右对齐，addWidget 靠左)
    ui->statusbar->addWidget(lblCommStatus);
    //ui->statusbar->addWidget(new QLabel(" | ", this)); // 分隔符
    ui->statusbar->addWidget(lblGlobalAlarm);//
    ui->statusbar->addPermanentWidget(lblTime);
    //设备状态流转：需要一个接口获取变量，emit信号去更新（传递一个字符串），在status链路去通知：包含按钮的按键状态，定时器启停，通信状态标签（由最开始的）。
    //五个状态：构造函数初始化，连接以后的状态，被操作以后禁用，操作完了重新启用，断开连接的状态

    ui->lblDoor->setText(" - ");
    ui->lblCompress->setText(" - ");
    ui->lblAlarm->setText(" - ");
    //ui->lcdTemp->setMode(QLCDNumber::Dec);

    //刷新时间戳
    timer = new QTimer(this);
    connect(timer,&QTimer::timeout,this,[=](){
        QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");
        lblTime->setText("当前时间：" + timeStr);
    });
    timer->start(1000);

    connect(ui->modeList,&QComboBox::currentTextChanged,[=](QString text){
        if(text == "串口通信"){
            ui->stackedWidget->setCurrentIndex(0);
        }else{
            ui->stackedWidget->setCurrentIndex(1);
        }
    });

    ui->mainStkWidget->setCurrentIndex(0);
    connect(ui->btn1,&QPushButton::clicked,[=](){
        ui->mainStkWidget->setCurrentIndex(0);
    });

    connect(ui->btn2,&QPushButton::clicked,[=](){
        ui->mainStkWidget->setCurrentIndex(1);
    });

    connect(ui->btn3,&QPushButton::clicked,[=](){
        ui->mainStkWidget->setCurrentIndex(2);
    });

    DeviceManager *device = new DeviceManager(this);

    connect(device,&DeviceManager::statusChanged,this,&MainWindow::onStatusChanged);

    //分层日志
    connect(device,&DeviceManager::logBusiness, this, &MainWindow::writeLog);

    // connect(device,&DeviceManager::errorOccurred,this,[=](QString errorMsg){//指定this
    //     QMessageBox::critical(this,"错误", errorMsg);
    // });

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

    connect(ui->btnSetTemp,&QPushButton::clicked,[=](){//...
        // double val = ui->targetTemp->value();
        // short sendVal = static_cast<short>(val*10);//定点数传输
        // QByteArray data;
        // data.append(static_cast<char>(sendVal>>8));//?
        // data.append(static_cast<char>(sendVal & 0xFF));
        // emit signalSendData(FUNC_WRITE_PARAM,data);
        // QString cleanLog = QString("下发目标温度：%1 ℃").arg(val);//业务日志
        // writeLog(cleanLog,true);//C2137
        ConfigData config;
        config.targetTemperature = 5;
        config.tempHighLimit     = 2;
        config.tempLowLimit      = 8;
        config.targetHumidity    = 50;
        config.humidHighLimit    = 75;
        config.humidLowLimit     = 35;
        device->requestWriteParam(config);
        //device->requestReadParam();
        //device->requestCmd();

    });

    //connect(ui->btnHistory,&QPushButton::clicked,[=](){
        // HistoryDialog dlg(this);
        // dlg.exec(); // 模态显示
        //HistoryDialog* dialog = new HistoryDialog(this);
        //查询
        connect(ui->historyWidget, &HistoryWidget::sigRequestHistory, device, &DeviceManager::sigQueryDbHistory);
        //接收
        connect(device, &DeviceManager::sigDbHistoryReady, ui->historyWidget, &HistoryWidget::onReceiveHistoryData);

        //dialog->setAttribute(Qt::WA_DeleteOnClose); // 关掉窗口自动销毁内存
       // dialog->show();
    //});

    refreshTimer = new QTimer(this);
    connect(refreshTimer,&QTimer::timeout,this,[=](){

       //if(device->state != DeviceState::Connected) return;

        DeviceData curData = device->getLatestData();
        //QString globalAlarmText = "";

        // 1. 更新温度和湿度
        ui->lcdTemp->display(QString::number(curData.actualTemperature,'f',1));
        ui->lcdHum->display(QString::number(curData.actualHumidity, 'f', 1));

        if(device->m_isTempSoftAlarm) {
            ui->lcdTemp->setStyleSheet("color: red; background-color: black;");
        } else {
            // 没报警，恢复正常颜色
            ui->lcdTemp->setStyleSheet("color: #00FF00; background-color: black;"); // 绿色
        }

        if (device->m_isHumSoftAlarm) {
            ui->lcdHum->setStyleSheet("color: red; background-color: black;");
        } else {
            ui->lcdHum->setStyleSheet("color: #00FFFF; background-color: black;"); // 蓝色
        }

        // 2. 更新箱门状态 (0-已关紧, 1-未关紧)
        if (curData.doorStatus == 0) {
            ui->lblDoor->setText("已关紧");
            ui->lblDoor->setStyleSheet("color: green;");
        } else {
            ui->lblDoor->setText("未关紧！");
            ui->lblDoor->setStyleSheet("color: red; font-weight: bold;");
        }

        // 3. 更新压缩机状态 (0-待机, 1-制冷中)
        if (curData.compressorStatus == 0) {
            ui->lblCompress->setText("待机中");
            ui->lblCompress->setStyleSheet("color: gray;");
        } else {
            ui->lblCompress->setText("制冷中 ❄️");
            ui->lblCompress->setStyleSheet("color: #0078D7; font-weight: bold;");
        }

        // 4. 更新系统报警状态
        if (curData.alarmCode == 0) {
            ui->lblAlarm->setText("系统正常运行");
            ui->lblAlarm->setStyleSheet("color: green;");
        } else {
            // 发生故障
            ui->lblAlarm->setText(QString("系统告警！故障码: %1").arg(curData.alarmCode));
            ui->lblAlarm->setStyleSheet("background-color: red; color: white; font-weight: bold; border-radius: 4px; padding: 2px;");
        }
        //重绘实时曲线
        //updatePlot(curData.actualTemperature);
        onDataReceived(curData);

    });
    refreshTimer->setInterval(50);//50ms触发，而下位机1s才发一次，读写分离，暂时多次读取同一数据

    //加载配置
    QSettings settings("config.ini", QSettings::IniFormat);
    QString lastPort = settings.value("PortName").toString();
    QString lastBaud = settings.value("Baud").toString();
    QString lastIp   = settings.value("IP").toString();
    QString port     = settings.value("Port").toString();
    double lastTemp  = settings.value("TargetTemp",0.0).toDouble();

    int idx1 = ui->portList->findText(lastPort);
    if(idx1 != -1) ui->portList->setCurrentIndex(idx1);
    int idx2 = ui->baudList->findText(lastBaud);
    if(idx2 != -1) ui->baudList->setCurrentIndex(idx2);

    ui->ipEdit->setText(lastIp);
    ui->portEdit->setText(port);
    ui->targetTemp->setValue(lastTemp);

    restoreGeometry(settings.value("Geometry").toByteArray());
}

MainWindow::~MainWindow()
{
    timer->stop();
    delete ui;
}

void MainWindow::onStatusChanged(DeviceState state){

    switch (state) {
    case DeviceState::Disconnected:
        // ui->lblConnStatus->setText("未连接");
        // ui->lblConnStatus->setStyleSheet("color: red; font-weight: bold;");
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

        lblCommStatus->setText("🔴 设备未连接");//

        ui->lblDoor->setText(" - ");
        ui->lblCompress->setText(" - ");
        ui->lblAlarm->setText(" - ");

        ui->btnOpen->setEnabled(true);
        ui->btnClose->setEnabled(false);
        ui->btnSetTemp->setEnabled(false);
        refreshTimer->stop();
        break;

    case DeviceState::Connecting:
        lblCommStatus->setText("🟡 连接中...");//
        break;

    case DeviceState::Reconnecting:
        // ui->lblConnStatus->setText("连接中/重连中...");
        // ui->lblConnStatus->setStyleSheet("color: orange; font-weight: bold;");

        lblCommStatus->setText("🟡 连接不稳定，尝试恢复...");//
        break;

    case DeviceState::Connected:
        // ui->lblConnStatus->setText("已连接");
        // ui->lblConnStatus->setStyleSheet("color: green; font-weight: bold;");
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

        lblCommStatus->setText("🟢 设备已在线");//

        ui->btnOpen->setEnabled(false);
        ui->btnClose->setEnabled(true);
        ui->btnSetTemp->setEnabled(true);
        refreshTimer->start();
        break;

    case DeviceState::Error:
        // ui->lblConnStatus->setText("设备异常");
        // ui->lblConnStatus->setStyleSheet("color: darkred; background-color: yellow;");
        lblCommStatus->setText("🔴 设备异常 (最大重试次数已满)");//
        break;
    }


    // if(state){
    //     ui->lblStatus->setText("已连接");
    //     ui->lblStatus->setStyleSheet("color: green;");
    //     ui->lblLight->setStyleSheet(R"(QLabel {
    //         min-width: 16px;
    //         min-height: 16px;
    //         max-width: 16px;
    //         max-height: 16px;
    //         background-color: green;
    //         border-radius: 8px;
    //         border: 1px solid #666;
    //     })");


    // }else{
    //     ui->lblStatus->setText("未连接");
    //     ui->lblStatus->setStyleSheet("color: red;");
    //     ui->lblLight->setStyleSheet(R"(QLabel {
    //         min-width: 16px;
    //         min-height: 16px;
    //         max-width: 16px;
    //         max-height: 16px;
    //         background-color: red;
    //         border-radius: 8px;
    //         border: 1px solid #666;
    //     })");

    // }

}

// void MainWindow::updateRealTimeUI(const DeviceData &data){

// }

void MainWindow::onDataReceived(const DeviceData &data){

    // 获取当前时间戳 (秒)
    double key = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
    // 添加数据点
    ui->plotTemp->graph(0)->addData(key, data.actualTemperature);
    ui->plotTemp->graph(1)->addData(key, data.actualHumidity);
    // 移除太老的数据 (比如只保留最近 100 个点，防止内存爆掉)
    //ui->plotTemp->graph(0)->data()->removeBefore(key - 100);
    //ui->plotTemp->graph(1)->data()->removeBefore(key - 100);
    // 实现自动滚动，最右侧边界key，跨度60s
    ui->plotTemp->xAxis->setRange(key, 60, Qt::AlignRight); // 显示最近60秒

    ui->plotTemp->graph(0)->rescaleAxes();
    // 湿度自动缩放右侧Y轴，且不影响X轴的缩放 true
    ui->plotTemp->graph(1)->rescaleAxes(true);
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

void MainWindow::writeLog(const QString& level, const QString& msg){

    QString timeStr = QDateTime::currentDateTime().toString("[HH:mm:ss.zzz]");

    // 分级日志
    QString htmlLog;
    if (level == "DEBUG") {
        if(!ui->chkHexDisplay->isChecked()) return;
        htmlLog = QString("<font color='gray'>%1 [%2] %3</b></font>").arg(timeStr, level, msg);
    }
    else if (level == "INFO") {
        htmlLog = QString("<font color='green'>%1 [%2] %3</font>").arg(timeStr, level, msg);
    }
    else if (level == "WARNING") {
        htmlLog = QString("<font color='orange'><b>%1 [%2] %3</b></font>").arg(timeStr, level, msg);
    }
    else if (level == "ERROR") {
        htmlLog = QString("<font color='red'><b>%1 [%2] %3</b></font>").arg(timeStr, level, msg);
        lblGlobalAlarm->setText(htmlLog);
    }

    // 追加到QTextBrowser中
    ui->txtLog->insertHtml(htmlLog + "<br>"); // <br>实现换行
    ui->txtLog->ensureCursorVisible();

    // 追加后检查行数，超过1000行则删除首行
    QTextDocument* doc = ui->txtLog->document();
    if (doc->blockCount() > 1000) {
        QTextCursor cursor(doc->findBlockByLineNumber(0));
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar(); // 删除换行符
    }
}

void MainWindow::initChart(){

    // 添加温度曲线
    ui->plotTemp->addGraph();
    ui->plotTemp->graph(0)->setPen(QPen(Qt::red, 2)); // 红色，线宽 2
    ui->plotTemp->graph(0)->setName("温度 (℃)");
    ui->plotTemp->yAxis->setLabel("温度 (℃)");
    ui->plotTemp->yAxis->setLabelColor(Qt::red);
    ui->plotTemp->yAxis->setRange(0, 50);

    // 添加湿度曲线
    ui->plotTemp->addGraph(ui->plotTemp->xAxis, ui->plotTemp->yAxis2);
    ui->plotTemp->graph(1)->setPen(QPen(Qt::blue, 2)); // 蓝色，线宽 2
    ui->plotTemp->graph(1)->setName("湿度 (%)");
    ui->plotTemp->yAxis2->setVisible(true); // 显示右侧 Y 轴
    ui->plotTemp->yAxis2->setLabel("湿度 (%)");
    ui->plotTemp->yAxis2->setLabelColor(Qt::blue);

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("MM-dd\nHH:mm:ss"); // 时间显示格式
    ui->plotTemp->xAxis->setTicker(dateTicker);
    ui->plotTemp->xAxis->setLabel("时间");


    // 显示图例
    ui->plotTemp->legend->setVisible(true);

    ui->plotTemp->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

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
