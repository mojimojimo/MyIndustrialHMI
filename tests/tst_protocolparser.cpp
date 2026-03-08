#include <QtTest>
#include "protocolparser.h"

/*
 * 测试目标 = 验证 ProtocolParser 在收到合法数据时是否能够正确解析并发出 frameReceived 信号
*/

class TestProtocolParser : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase();

    void test_validFrame();
    void test_checksumError();
    void test_tailError();
    void test_halfPacket();
    void test_stickyPackets();
    void test_noiseBeforeFrame();
    void test_maxPayload();
    void test_illegalLength();
};

void TestProtocolParser::initTestCase()
{
    qRegisterMetaType<Frame>("Frame");//注册元类型Frame
}

// 测试1：合法帧解析
void TestProtocolParser::test_validFrame()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::frameReceived);

    //封包
    QByteArray frame;

    frame.append((char)0xAA);
    frame.append((char)0x55);
    frame.append((char)FUNC_TEMP_DATA);
    frame.append((char)2);
    frame.append((char)0x11);
    frame.append((char)0x22);

    unsigned char sum = FUNC_TEMP_DATA + 2 + 0x11 + 0x22;

    frame.append(sum);
    frame.append((char)0xFF);

    //解析数据 processData()
    parser.onRawDataReceived(frame);

    QCOMPARE(spy.count(), 1); //验证发出 frameReceived 信号

    //验证参数是否正确
    QList<QVariant> arguments = spy.takeFirst();
    Frame received = qvariant_cast<Frame>(arguments.at(0));

    QCOMPARE(received.funcCode, (quint8)FUNC_TEMP_DATA);
    QCOMPARE(received.payload.size(), 2);
}

// 测试2：校验和错误
void TestProtocolParser::test_checksumError()
{
    ProtocolParser parser;

    QSignalSpy spy(&parser, &ProtocolParser::frameReceived);

    QByteArray frame;

    frame.append((char)0xAA);
    frame.append((char)0x55);
    frame.append((char)FUNC_TEMP_DATA);
    frame.append((char)1);
    frame.append((char)0x33);

    frame.append((char)0x00);   // 错误校验
    frame.append((char)0xFF);

    parser.onRawDataReceived(frame);

    QCOMPARE(spy.count(), 0);
}

// 测试3：帧尾错误
void TestProtocolParser::test_tailError()
{
    ProtocolParser parser;

    QSignalSpy spy(&parser, &ProtocolParser::frameReceived);

    QByteArray frame;

    frame.append((char)0xAA);
    frame.append((char)0x55);
    frame.append((char)FUNC_TEMP_DATA);
    frame.append((char)1);
    frame.append((char)0x10);

    unsigned char sum = FUNC_TEMP_DATA + 1 + 0x10;

    frame.append(sum);
    frame.append((char)0x00); // 错误尾

    parser.onRawDataReceived(frame);

    QCOMPARE(spy.count(), 0);
}

// 测试4：半包
void TestProtocolParser::test_halfPacket()
{
    ProtocolParser parser;

    QSignalSpy spy(&parser, &ProtocolParser::frameReceived);

    QByteArray part1;

    part1.append((char)0xAA);
    part1.append((char)0x55);
    part1.append((char)FUNC_TEMP_DATA);
    part1.append((char)1);

    parser.onRawDataReceived(part1);

    QCOMPARE(spy.count(), 0);

    QByteArray part2;

    part2.append((char)0x44);

    unsigned char sum = FUNC_TEMP_DATA + 1 + 0x44;

    part2.append(sum);
    part2.append((char)0xFF);

    parser.onRawDataReceived(part2);

    QCOMPARE(spy.count(), 1);
}

// 测试5：粘包（连续两帧）
void TestProtocolParser::test_stickyPackets()
{
    ProtocolParser parser;

    QSignalSpy spy(&parser, &ProtocolParser::frameReceived);

    QByteArray data;

    for(int i=0;i<2;i++)
    {
        QByteArray frame;

        frame.append((char)0xAA);
        frame.append((char)0x55);
        frame.append((char)FUNC_TEMP_DATA);
        frame.append((char)1);
        frame.append((char)(0x10 + i));

        unsigned char sum = FUNC_TEMP_DATA + 1 + (0x10 + i);

        frame.append(sum);
        frame.append((char)0xFF);

        data.append(frame);
    }

    parser.onRawDataReceived(data);

    QCOMPARE(spy.count(), 2);
}


// 测试6：脏数据 + 合法帧
void TestProtocolParser::test_noiseBeforeFrame()
{
    ProtocolParser parser;

    QSignalSpy spy(&parser, &ProtocolParser::frameReceived);

    QByteArray data;

    data.append((char)0x99);
    data.append((char)0x88);
    data.append((char)0x77);

    data.append((char)0xAA);
    data.append((char)0x55);
    data.append((char)FUNC_TEMP_DATA);
    data.append((char)1);
    data.append((char)0x66);

    unsigned char sum = FUNC_TEMP_DATA + 1 + 0x66;

    data.append(sum);
    data.append((char)0xFF);

    parser.onRawDataReceived(data);

    QCOMPARE(spy.count(), 1);
}


// 测试7：最大payload
void TestProtocolParser::test_maxPayload()
{
    ProtocolParser parser;

    QSignalSpy spy(&parser, &ProtocolParser::frameReceived);

    QByteArray payload;

    for(int i=0;i<PROTOCOL_MAX_DATALEN;i++)
        payload.append((char)i);

    QByteArray frame;

    frame.append((char)0xAA);
    frame.append((char)0x55);
    frame.append((char)FUNC_TEMP_DATA);
    frame.append((char)payload.size());

    frame.append(payload);

    unsigned char sum = FUNC_TEMP_DATA + payload.size();

    for(auto b : payload)
        sum += (unsigned char)b;

    frame.append(sum);
    frame.append((char)0xFF);

    parser.onRawDataReceived(frame);

    QCOMPARE(spy.count(), 1);
}

// 测试8：非法长度
void TestProtocolParser::test_illegalLength()
{
    ProtocolParser parser;

    QSignalSpy spy(&parser, &ProtocolParser::frameReceived);

    QByteArray frame;

    frame.append((char)0xAA);
    frame.append((char)0x55);
    frame.append((char)FUNC_TEMP_DATA);

    frame.append((char)(PROTOCOL_MAX_DATALEN + 10)); // 非法长度

    frame.append((char)0x11);

    frame.append((char)0x00);
    frame.append((char)0xFF);

    parser.onRawDataReceived(frame);

    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestProtocolParser)
#include "tst_protocolparser.moc"
