#include "databasemanager.h"
#include <QDebug>
#include <QDateTime>
#include <QSqlError>

bool DatabaseManager::init(){

    //每个数据库连接只能在创建它的线程中使用
    m_db = QSqlDatabase::addDatabase("QSQLITE","MedicalDbConnection");
    m_db.setDatabaseName("medical_sensor.db");

    if(!m_db.open()){
        qDebug()<<"Error:Failed to connect database."<<m_db.lastError();
        return false;
    }

    QSqlQuery query(m_db);

    // 创建环境数据表（温湿度）
    QString createEnvTable = R"(
        create table if not exists env_history(
            id integer primary key autoincrement,
            timestamp datetime default CURRENT_TIMESTAMP,
            temperature REAL,
            humidity REAL
        )
    )";
    if(!query.exec(createEnvTable)){
        qDebug()<<"Create table env_history error:"<<query.lastError();
        return false;
    }

    //创建突发事件表
    QString createEventTable = R"(
        create table if not exists event_logs(
            id integer primary key autoincrement,
            timestamp datetime default CURRENT_TIMESTAMP,
            event_type VARCHAR(20),
            description VARCHAR(100)
        )
    )";
    if(!query.exec(createEventTable)){
        qDebug()<<"Create table event_logs error:"<<query.lastError();
        return false;
    }

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(50);
    connect(m_flushTimer, &QTimer::timeout, this, &DatabaseManager::commitBatchInsert);
    m_flushTimer->start();

    qDebug()<<"DB is ok!";
    return true;
}

void DatabaseManager::onInsertEnvData(double temp, double hum){

    m_envBuffer.append({temp, hum});
    if (shouldFlushNow()) {
        commitBatchInsert();
    }

}

void DatabaseManager::commitBatchInsert(){
    if (m_envBuffer.isEmpty()) return;

    if (!m_db.transaction()) {
        qDebug() << "[DB] start transaction failed:" << m_db.lastError();
        return;
    }
    //auto timestamp = QDateTime::currentDateTime();
    //sql注入
    // if(! query.exec(QString("insert into temperature_records values (%1,%2);").arg(timestamp).arg(value))){
    //      qDebug()<<"Insert error:"<<query.lastError();
    // }

    QSqlQuery query(m_db);
    query.prepare("insert into env_history (temperature, humidity) values (?,?)");//参数绑定：分离解析
    //query.addBindValue(temp);
    //query.addBindValue(hum);
    for(const auto& record : m_envBuffer){
        query.addBindValue(record.temperature);
        query.addBindValue(record.humidity);
        if(!query.exec()){
            qDebug()<<"Batch insert error:"<<query.lastError();
            m_db.rollback(); // 回滚事务
            return;
        }
    }

    if(m_db.commit()){// 提交事务
        qDebug()<<"[DB] 成功批量写入: "<<m_envBuffer.size()<<" 条数据";
        m_envBuffer.clear(); // 只在提交成功后清空缓冲区
    } else {
        qDebug()<<"[DB] 批量写入失败:"<<m_db.lastError();
        m_db.rollback(); // 回滚事务
    }
}

void DatabaseManager::onInsertEvent(const QString &type, const QString &desc){
    QSqlQuery query(m_db);
    query.prepare("insert into event_logs (event_type, description) values (?,?)");//参数绑定：分离解析
    query.addBindValue(type);
    query.addBindValue(desc);

    if(!query.exec()){
        qDebug()<<"InsertEvent error:"<<query.lastError();
    }
}

void DatabaseManager::onQueryHistory(const QDateTime &start,const QDateTime& end){

    commitBatchInsert();// 确保查询前把缓冲区数据写入数据库

    QList<HistoryData> list;
    QSqlQuery query(m_db);//"  "

    query.prepare("SELECT timestamp, temperature, humidity FROM env_history "
                  "WHERE datetime(timestamp, 'localtime') BETWEEN :start AND :end ORDER BY timestamp ASC");
    query.bindValue(":start", start.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":end", end.toString("yyyy-MM-dd HH:mm:ss"));

    if (query.exec()) {
        while (query.next()) {
            HistoryData data;
            data.timestamp = query.value(0).toString();
            data.temperature = query.value(1).toDouble();
            data.humidity = query.value(2).toDouble();
            list.append(data);
        }
    } else {
        qWarning() << "查询历史记录失败:" << query.lastError();
    }

    emit sigHistoryDataReady(list);
}

DatabaseManager::~DatabaseManager()
{
    if(!m_envBuffer.isEmpty()){ // 确保退出前把缓冲区剩余数据写入数据库
        commitBatchInsert();
    }
    if (m_flushTimer) {
        m_flushTimer->stop();
    }
    if (m_db.isOpen()) {
        m_db.close();
    }

    // 移除数据库连接
    m_db = QSqlDatabase(); // 释放对连接的引用
    QSqlDatabase::removeDatabase("MedicalDbConnection");
}

bool DatabaseManager::shouldFlushNow() const
{
    return m_envBuffer.size() >= BATCH_SIZE;
}
