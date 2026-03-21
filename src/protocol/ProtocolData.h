#ifndef PROTOCOLDATA_H
#define PROTOCOLDATA_H

#include <QMetaType>

struct DeviceData{
    double actualTemperature = 0.0;  // 箱内主探头温度
    double actualHumidity = 0.0;     // 箱内湿度
    uint8_t doorStatus = 0;        // 箱门状态: 0-已关紧, 1-未关紧 (开门太久要报警)
    uint8_t compressorStatus = 0;  // 压缩机状态: 0-待机, 1-制冷中
    uint8_t alarmCode = 0;         // 故障码: 0-系统正常, 1-温度超限, 2-传感器脱落
};
Q_DECLARE_METATYPE(DeviceData) //注册元类型

struct ConfigData {
    double targetTemperature = 0.0;  // 目标温度
    double tempHighLimit = 0.0;      // 温度上限报警阈值
    double tempLowLimit = 0.0;       // 温度下限报警阈值

    double targetHumidity = 0.0;     // 目标湿度
    double humidHighLimit = 0.0;     // 湿度上限报警阈值
    double humidLowLimit = 0.0;      // 湿度下限报警阈值

    //uint8_t doorAlarmDelaySec; // 开门超时报警延时
};
Q_DECLARE_METATYPE(ConfigData)

// enum FuncCode : char{
//     FUNC_READ_TEMP = 0x03,//查询温度
//     FUNC_TEMP_DATA = 0x01,//温度返回
//     FUNC_SET_PARAM = 0x10 //下发参数
// };

enum FuncCode : quint8 {

    // 数据主动上报类
    FUNC_REPORT_ALL_DATA = 0x01,  // 下位机主动上报所有实时数据
    FUNC_PARAM_RETURN    = 0x21,   // 下位机回复的当前参数配置(payload:ConfigData)
    FUNC_CMD_ACK         = 0x80,   // 应答(payload:0x00)

    // 上位机下发控制类
    FUNC_WRITE_PARAM    = 0x10,  // 上位机下发参数 (payload:ConfigData)
    FUNC_CTRL_CMD       = 0x11,  // 上位机控制指令 (payload:远程消音0x01、强制化霜0x02)
    FUNC_READ_PARAM     = 0x20   // 上位机查询当前设定参数 (payload长度为0)

    //FUNC_READ_ALL_DATA  = 0x00 上位机主动轮询

};

const char FRAME_HEAD_1 = static_cast<char> (0xAA);//?const char and enum
const char FRAME_HEAD_2 = static_cast<char> (0x55);
const char FRAME_TAIL = static_cast<char> (0xFF);

const int PROTOCOL_MAX_DATALEN = 255;

struct Frame{
    quint8 funcCode;//
    QByteArray payload;
};
Q_DECLARE_METATYPE(Frame)

#endif // PROTOCOLDATA_H
