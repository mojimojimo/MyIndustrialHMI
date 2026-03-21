#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>//连接管家
#include <QSqlQuery>//执行者

struct HistoryData{
    QString timestamp;
    double temperature;
    double humidity;
};
Q_DECLARE_METATYPE(QList<HistoryData>)

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    // static DatabaseManager& instance(){
    //     static DatabaseManager instance;
    //     return instance;
    // }
    DatabaseManager(){};
    ~DatabaseManager();
    bool init();

public slots:
    void onInsertEnvData(double temp, double hum);
    void onInsertEvent(const QString &type, const QString &desc);
    void onQueryHistory(const QDateTime &start,const QDateTime& end);

signals:
    void sigHistoryDataReady(const QList<HistoryData>& dataList);

private:
    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
