#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>//连接管家
#include <QSqlQuery>//执行者

struct HistoryData{
    qint64 timestamp;
    double value;
};

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager& instance(){
        static DatabaseManager instance;
        return instance;
    }

    bool init();
    void insertData(double value);
    QList<HistoryData> queryHistory(const QDateTime &start,const QDateTime& end);

signals:

private:
    DatabaseManager(){};
    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
