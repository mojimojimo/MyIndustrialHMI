#include <QtTest>
#include <QSignalSpy>

#include "protocolparser.h"

static void appendU16BE(QByteArray &ba, quint16 value)
{
    ba.append(static_cast<char>((value >> 8) & 0xFF));
    ba.append(static_cast<char>(value & 0xFF));
}

static void appendS16BE(QByteArray &ba, qint16 value)
{
    appendU16BE(ba, static_cast<quint16>(value));
}

static QByteArray buildPacket(quint8 funcCode, const QByteArray &payload)
{
    QByteArray packet;
    packet.append(static_cast<char>(FRAME_HEAD_1));
    packet.append(static_cast<char>(FRAME_HEAD_2));
    packet.append(static_cast<char>(funcCode));
    packet.append(static_cast<char>(payload.size()));
    packet.append(payload);

    quint8 sum = 0;
    for (int i = 2; i < packet.size(); ++i) {
        sum = static_cast<quint8>(sum + static_cast<quint8>(packet.at(i)));
    }

    packet.append(static_cast<char>(sum));
    packet.append(static_cast<char>(FRAME_TAIL));
    return packet;
}

static QByteArray buildRealtimePayload(double temp, double hum,
                                       quint8 door, quint8 compressor, quint8 alarm)
{
    QByteArray payload;
    appendS16BE(payload, static_cast<qint16>(temp * 10.0));
    appendU16BE(payload, static_cast<quint16>(hum * 10.0));
    payload.append(static_cast<char>(door));
    payload.append(static_cast<char>(compressor));
    payload.append(static_cast<char>(alarm));
    return payload;
}

static QByteArray buildConfigPayload(double targetTemperature,
                                     double tempHighLimit,
                                     double tempLowLimit,
                                     double targetHumidity,
                                     double humidHighLimit,
                                     double humidLowLimit)
{
    QByteArray payload;
    appendS16BE(payload, static_cast<qint16>(targetTemperature * 10.0));
    appendS16BE(payload, static_cast<qint16>(tempHighLimit * 10.0));
    appendS16BE(payload, static_cast<qint16>(tempLowLimit * 10.0));
    appendU16BE(payload, static_cast<quint16>(targetHumidity * 10.0));
    appendU16BE(payload, static_cast<quint16>(humidHighLimit * 10.0));
    appendU16BE(payload, static_cast<quint16>(humidLowLimit * 10.0));
    return payload;
}

class TestProtocolParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void test_realtimeDataParsed();
    void test_configParamLoaded();
    void test_cmdAckReceived();

    void test_halfPacket();
    void test_stickyPackets();
    void test_noiseBeforeFrame();

    void test_onPackReadParam();
    void test_onPackWriteParam();
    void test_onPackCmd();
};

void TestProtocolParser::initTestCase()
{
    qRegisterMetaType<DeviceData>("DeviceData");
    qRegisterMetaType<ConfigData>("ConfigData");
}

void TestProtocolParser::test_realtimeDataParsed()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::RealtimeDataParsed);

    QByteArray payload = buildRealtimePayload(25.3, 56.7, 1, 0, 2);
    QByteArray frame = buildPacket(FUNC_REPORT_ALL_DATA, payload);

    parser.onRawDataReceived(frame);

    QCOMPARE(spy.count(), 1);

    const QList<QVariant> args = spy.takeFirst();
    DeviceData data = qvariant_cast<DeviceData>(args.at(0));

    QVERIFY(qAbs(data.actualTemperature - 25.3) < 0.0001);
    QVERIFY(qAbs(data.actualHumidity - 56.7) < 0.0001);
    QCOMPARE(data.doorStatus, static_cast<uint8_t>(1));
    QCOMPARE(data.compressorStatus, static_cast<uint8_t>(0));
    QCOMPARE(data.alarmCode, static_cast<uint8_t>(2));
}

void TestProtocolParser::test_configParamLoaded()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::configParamLoaded);

    QByteArray payload = buildConfigPayload(2.5, 8.0, 2.0, 55.5, 70.0, 40.0);
    QByteArray frame = buildPacket(FUNC_PARAM_RETURN, payload);

    parser.onRawDataReceived(frame);

    QCOMPARE(spy.count(), 1);

    const QList<QVariant> args = spy.takeFirst();
    ConfigData config = qvariant_cast<ConfigData>(args.at(0));

    QVERIFY(qAbs(config.targetTemperature - 2.5) < 0.0001);
    QVERIFY(qAbs(config.tempHighLimit - 8.0) < 0.0001);
    QVERIFY(qAbs(config.tempLowLimit - 2.0) < 0.0001);
    QVERIFY(qAbs(config.targetHumidity - 55.5) < 0.0001);
    QVERIFY(qAbs(config.humidHighLimit - 70.0) < 0.0001);
    QVERIFY(qAbs(config.humidLowLimit - 40.0) < 0.0001);
}

void TestProtocolParser::test_cmdAckReceived()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::cmdAckReceived);

    QByteArray payload;
    payload.append(static_cast<char>(0x00));   // ack
    QByteArray frame = buildPacket(FUNC_CMD_ACK, payload);

    parser.onRawDataReceived(frame);

    QCOMPARE(spy.count(), 1);

    const QList<QVariant> args = spy.takeFirst();
    const bool ack = args.at(0).toBool();
    const quint8 result = static_cast<quint8>(args.at(1).toUInt());

    QCOMPARE(ack, true);
    QCOMPARE(result, static_cast<quint8>(0x00));
}

void TestProtocolParser::test_halfPacket()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::RealtimeDataParsed);

    QByteArray payload = buildRealtimePayload(25.3, 56.7, 1, 0, 2);
    QByteArray frame = buildPacket(FUNC_REPORT_ALL_DATA, payload);

    // 先喂前半包
    QByteArray part1 = frame.left(5);
    QByteArray part2 = frame.mid(5);

    parser.onRawDataReceived(part1);
    QCOMPARE(spy.count(), 0);

    parser.onRawDataReceived(part2);
    QCOMPARE(spy.count(), 1);
}

void TestProtocolParser::test_stickyPackets()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::RealtimeDataParsed);

    QByteArray payload1 = buildRealtimePayload(25.3, 56.7, 1, 0, 2);
    QByteArray payload2 = buildRealtimePayload(26.1, 57.2, 0, 1, 0);

    QByteArray frame1 = buildPacket(FUNC_REPORT_ALL_DATA, payload1);
    QByteArray frame2 = buildPacket(FUNC_REPORT_ALL_DATA, payload2);

    QByteArray sticky = frame1 + frame2;

    parser.onRawDataReceived(sticky);

    QCOMPARE(spy.count(), 2);
}

void TestProtocolParser::test_noiseBeforeFrame()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::RealtimeDataParsed);

    QByteArray payload = buildRealtimePayload(25.3, 56.7, 1, 0, 2);
    QByteArray frame = buildPacket(FUNC_REPORT_ALL_DATA, payload);

    QByteArray noisyData;
    noisyData.append(static_cast<char>(0x99));
    noisyData.append(static_cast<char>(0x88));
    noisyData.append(static_cast<char>(0x77));
    noisyData.append(frame);

    parser.onRawDataReceived(noisyData);

    QCOMPARE(spy.count(), 1);
}

void TestProtocolParser::test_onPackReadParam()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::sendRawData);

    parser.onPackReadParam();

    QCOMPARE(spy.count(), 1);

    QByteArray sent = spy.takeFirst().at(0).toByteArray();

    QByteArray expected;
    expected.append(static_cast<char>(FRAME_HEAD_1));
    expected.append(static_cast<char>(FRAME_HEAD_2));
    expected.append(static_cast<char>(FUNC_READ_PARAM));
    expected.append(static_cast<char>(0x00));
    expected.append(static_cast<char>(FUNC_READ_PARAM)); // checksum = func + len
    expected.append(static_cast<char>(FRAME_TAIL));

    QCOMPARE(sent, expected);
}

void TestProtocolParser::test_onPackWriteParam()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::sendRawData);

    ConfigData config;
    config.targetTemperature = 2.5;
    config.tempHighLimit = 8.0;
    config.tempLowLimit = 2.0;
    config.targetHumidity = 55.5;
    config.humidHighLimit = 70.0;
    config.humidLowLimit = 40.0;

    parser.onPackWriteParam(config);

    QCOMPARE(spy.count(), 1);

    QByteArray sent = spy.takeFirst().at(0).toByteArray();

    QByteArray payload = buildConfigPayload(2.5, 8.0, 2.0, 55.5, 70.0, 40.0);
    QByteArray expected = buildPacket(FUNC_WRITE_PARAM, payload);

    QCOMPARE(sent, expected);
}

void TestProtocolParser::test_onPackCmd()
{
    ProtocolParser parser;
    QSignalSpy spy(&parser, &ProtocolParser::sendRawData);

    parser.onPackCmd("01");   // payload = 0x01

    QCOMPARE(spy.count(), 1);

    QByteArray sent = spy.takeFirst().at(0).toByteArray();

    QByteArray payload;
    payload.append(static_cast<char>(0x01));

    QByteArray expected = buildPacket(FUNC_CTRL_CMD, payload);

    QCOMPARE(sent, expected);
}

QTEST_MAIN(TestProtocolParser)
#include "tst_protocolparser.moc"
