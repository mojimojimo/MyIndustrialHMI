#include "databasemanager.h"
#include <QDebug>
#include <QDateTime>
#include <QSqlError>

bool DatabaseManager::init(){
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName("sensor_data.db");

    if(!m_db.open()){
        qDebug()<<"Error:Failed to connect database."<<m_db.lastError();
        return false;
    }

    QSqlQuery query;
    QString createTable = R"(
        create table if not exists temperature_records(
            id integer primary key autoincrement,
            timestamp datetime,
            value real
        )
    )";

    if(!query.exec(createTable)){
        qDebug()<<"Create table error:"<<query.lastError();
        return false;
    }
    return true;
}

void DatabaseManager::insertData(double value){

    //auto timestamp = QDateTime::currentDateTime();
    //sql注入
    // if(! query.exec(QString("insert into temperature_records values (%1,%2);").arg(timestamp).arg(value))){
    //      qDebug()<<"Insert error:"<<query.lastError();
    // }

    QSqlQuery query;
    query.prepare("insert into temperature_records(timestamp, value) values (?,?)");//参数绑定：分离解析
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(value);
    if(!query.exec()){
        qDebug()<<"Insert error:"<<query.lastError();
    }
}

QList<HistoryData> DatabaseManager::queryHistory(const QDateTime &start,const QDateTime& end){
    QList<HistoryData> list;
    QSqlQuery query;//"  "
    query.prepare("select timestamp,value from temperature_records "
                  "where timestamp between ? and ? order by timestamp asc");
    query.addBindValue(start);
    query.addBindValue(end);

    if(query.exec()){
        while(query.next()){
            HistoryData data;
            QDateTime dt = query.value(0).toDateTime();
            data.timestamp = dt.toMSecsSinceEpoch();
            data.value = query.value(1).toDouble();
            list.append(data);
        }

    }else{
        qDebug()<<"Query history error:"<<query.lastError();
    }
    return list;
}
