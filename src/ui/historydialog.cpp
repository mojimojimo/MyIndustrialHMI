#include "historydialog.h"
#include "ui_historydialog.h"
#include "databasemanager.h"
#include "qcustomplot.h"

HistoryDialog::HistoryDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HistoryDialog)
{
    ui->setupUi(this);

    // 默认查询过去 1 小时
    ui->dtEnd->setDateTime(QDateTime::currentDateTime());
    ui->dtStart->setDateTime(QDateTime::currentDateTime().addSecs(-3600));

    // 初始化QCustomPlot
    initChart();
}

HistoryDialog::~HistoryDialog()
{
    delete ui;
}

void HistoryDialog::initChart(){
    ui->plotHistory->addGraph();
    ui->plotHistory->graph(0)->setPen(QPen(Qt::blue));

    ui->plotHistory->xAxis->setLabel("时间 (s)");
    ui->plotHistory->yAxis->setLabel("温度 (℃)");

    ui->plotHistory->yAxis->setRange(-50, 50);

    ui->plotHistory->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("HH:mm:ss");
    ui->plotHistory->xAxis->setTicker(dateTicker);
    ui->plotHistory->xAxis->setTickLabelRotation(30);
}

void HistoryDialog::on_btnQuery_clicked() {
    QDateTime start = ui->dtStart->dateTime();
    QDateTime end = ui->dtEnd->dateTime();

    //查询数据
    auto dataList = DatabaseManager::instance().queryHistory(start, end);

    if (dataList.isEmpty()) {
        QMessageBox::information(this, "提示", "该时间段无数据！");
        return;
    }

    //转换数据给QCustomPlot
    QVector<double> x(dataList.size()), y(dataList.size());
    for (int i = 0; i < dataList.size(); ++i) {
        x[i] = dataList[i].timestamp / 1000.0; //时间轴单位是秒
        y[i] = dataList[i].value;
    }

    //绘图
    ui->plotHistory->graph(0)->setData(x, y);
    ui->plotHistory->rescaleAxes();
    ui->plotHistory->replot();
}
