#ifndef TCPWORKER_H
#define TCPWORKER_H

#include "commworker.h"
#include <QTcpSocket>

class TcpWorker : public CommWorker
{
    Q_OBJECT
public:
    explicit TcpWorker(CommWorker *parent = nullptr);
    ~TcpWorker();

public slots:
    void open(QString target,int portOrBaud) override;
    void close() override;
    void sendData(const QByteArray &data) override;

private slots:
    void onReadyRead();

signals:

private:
    QTcpSocket *socket = nullptr;
};

#endif // TCPWORKER_H
