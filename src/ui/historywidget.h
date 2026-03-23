#ifndef HISTORYWIDGET_H
#define HISTORYWIDGET_H

#include <QWidget>
#include "databasemanager.h"

namespace Ui {
class HistoryWidget;
}

class HistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWidget(QWidget *parent = nullptr);
    ~HistoryWidget();

public slots:
    void onReceiveHistoryData(const QList<HistoryData>& dataList);

private slots:
    void on_btnQuery_clicked();
    void on_btnExportCSV_clicked();

signals:
    void sigRequestHistory(const QDateTime &start,const QDateTime& end);

private:
    Ui::HistoryWidget *ui;
    QList<HistoryData> m_curDataList;
    //void initChart();
    void initPlot();
    void updatePlot(const QList<HistoryData>& dataList);

};

#endif // HISTORYWIDGET_H
