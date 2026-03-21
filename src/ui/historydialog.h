#ifndef HISTORYDIALOG_H
#define HISTORYDIALOG_H

#include <QDialog>
#include "databasemanager.h"

namespace Ui {
class HistoryDialog;
}

class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(QWidget *parent = nullptr);
    ~HistoryDialog();

public slots:
    void onReceiveHistoryData(const QList<HistoryData>& dataList);

private slots:
    void on_btnQuery_clicked();
    void on_btnExportCSV_clicked();

signals:
    void sigRequestHistory(const QDateTime &start,const QDateTime& end);

private:
    Ui::HistoryDialog *ui;
    QList<HistoryData> m_curDataList;
    //void initChart();
    void initPlot();
    void updatePlot(const QList<HistoryData>& dataList);

};

#endif // HISTORYDIALOG_H
