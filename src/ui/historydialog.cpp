#include "historydialog.h"
#include "ui_historydialog.h"
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
    //initChart();
    initPlot();
}

HistoryDialog::~HistoryDialog()
{
    delete ui;
}

// void HistoryDialog::initChart(){
//     ui->plotHistory->addGraph();
//     ui->plotHistory->graph(0)->setPen(QPen(Qt::blue));

//     ui->plotHistory->xAxis->setLabel("时间 (s)");
//     ui->plotHistory->yAxis->setLabel("温度 (℃)");

//     ui->plotHistory->yAxis->setRange(-50, 50);

//     ui->plotHistory->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

//     QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
//     dateTicker->setDateTimeFormat("HH:mm:ss");
//     ui->plotHistory->xAxis->setTicker(dateTicker);
//     ui->plotHistory->xAxis->setTickLabelRotation(30);
// }

void HistoryDialog::initPlot() {
    // 添加温度曲线
    ui->plotHistory->addGraph();
    ui->plotHistory->graph(0)->setPen(QPen(Qt::red, 2)); // 红色，线宽 2
    ui->plotHistory->graph(0)->setName("温度 (℃)");
    ui->plotHistory->yAxis->setLabel("温度 (℃)");
    ui->plotHistory->yAxis->setLabelColor(Qt::red);

    // 添加湿度曲线
    ui->plotHistory->addGraph(ui->plotHistory->xAxis, ui->plotHistory->yAxis2);
    ui->plotHistory->graph(1)->setPen(QPen(Qt::blue, 2)); // 蓝色，线宽 2
    ui->plotHistory->graph(1)->setName("湿度 (%)");
    ui->plotHistory->yAxis2->setVisible(true); // 显示右侧 Y 轴
    ui->plotHistory->yAxis2->setLabel("湿度 (%)");
    ui->plotHistory->yAxis2->setLabelColor(Qt::blue);

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("MM-dd\nHH:mm:ss"); // 时间显示格式
    ui->plotHistory->xAxis->setTicker(dateTicker);
    ui->plotHistory->xAxis->setLabel("时间");


    // 显示图例
    ui->plotHistory->legend->setVisible(true);

    ui->plotHistory->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void HistoryDialog::updatePlot(const QList<HistoryData>& dataList) {
    // 无数据清空图表
    if (dataList.isEmpty()) {
        ui->plotHistory->graph(0)->data()->clear();
        ui->plotHistory->graph(1)->data()->clear();
        ui->plotHistory->replot();
        return;
    }

    QVector<double> xTime; // 存时间戳
    QVector<double> yTemp; // 存温度
    QVector<double> yHum;  // 存湿度

    for (const auto& data : dataList) {
        QDateTime dt = QDateTime::fromString(data.timestamp, "yyyy-MM-dd HH:mm:ss");
        dt.setTimeSpec(Qt::UTC); // 明确时区为UTC

        xTime.append(dt.toSecsSinceEpoch()); // 转换为秒级时间戳

        yTemp.append(data.temperature);
        yHum.append(data.humidity);
    }

    ui->plotHistory->graph(0)->setData(xTime, yTemp); // 曲线0：温度
    ui->plotHistory->graph(1)->setData(xTime, yHum);  // 曲线1：湿度

    ui->plotHistory->graph(0)->rescaleAxes();
    // 湿度自动缩放右侧Y轴，且不影响X轴的缩放 true
    ui->plotHistory->graph(1)->rescaleAxes(true);

    ui->plotHistory->replot();
}

void HistoryDialog::on_btnQuery_clicked() {
    QDateTime start = ui->dtStart->dateTime();
    QDateTime end = ui->dtEnd->dateTime();

    ui->btnQuery->setEnabled(false);
    ui->lblStatus->setText("正在后台检索数据，请稍候...");

    // 发出查询请求信号 (去往子线程的 DbWorker)?
    emit sigRequestHistory(start, end);
}

void HistoryDialog::onReceiveHistoryData(const QList<HistoryData>& dataList) {
    // 恢复 UI 状态
    ui->btnQuery->setEnabled(true);
    ui->lblStatus->setText(QString("查询完成，共找到 %1 条记录。").arg(dataList.size()));

    if (dataList.isEmpty()) {
        QMessageBox::information(this, "提示", "该时间段无数据！");
        return;
    }

    m_curDataList = dataList;

    updatePlot(dataList);
}

void HistoryDialog::on_btnExportCSV_clicked()
{
    if (m_curDataList.isEmpty()) {
        QMessageBox::warning(this, "提示", "当前没有可导出的数据，请先查询！");
        return;
    }

    // 保存位置
    QString defaultName = QString("冷藏箱历史数据_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString fileName = QFileDialog::getSaveFileName(this, "导出 CSV 报表", defaultName, "CSV 文件 (*.csv)");

    if (fileName.isEmpty()) return; // 用户取消

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法创建文件！");
        return;
    }

    QTextStream out(&file);

    out.setEncoding(QStringConverter::Utf8);// 解决Excel打开CSV中文乱码
    out << "\xEF\xBB\xBF";
    out << "记录时间,温度(℃),湿度(%)\n"; // 写入表头

    for (const auto& data : m_curDataList) {

        // 将 UTC 时间字符串转换为本地时间
        QDateTime utcDt = QDateTime::fromString(data.timestamp, "yyyy-MM-dd HH:mm:ss");
        utcDt.setTimeSpec(Qt::UTC);
        QString localTimeStr = utcDt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss");

        out << localTimeStr << ","
            << data.temperature << ","
            << data.humidity << "\n";
    }

    file.close();
    QMessageBox::information(this, "成功", "数据报表导出成功！");
}

