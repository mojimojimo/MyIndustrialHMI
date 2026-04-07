#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QSqlDatabase>//连接管家
#include <QSqlQuery>//执行者

struct HistoryData{
    QString timestamp;
    double temperature;
    double humidity;
};

Q_DECLARE_METATYPE(QList<HistoryData>)

struct EnvRecord{
    double temperature;
    double humidity;
};

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
    QList<EnvRecord> m_envBuffer; // 环境数据缓冲区
    const int BATCH_SIZE = 20; // 批量插入的大小
    void commitBatchInsert();
    bool shouldFlushNow() const;
    QTimer *m_flushTimer = nullptr;//定时器，定期检查是否需要批量插入
};

#endif // DATABASEMANAGER_H
