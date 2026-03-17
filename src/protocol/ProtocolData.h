#ifndef PROTOCOLDATA_H
#define PROTOCOLDATA_H

#include <QMetaType>

struct DeviceData{
    double actualTemperature;  // 箱内主探头温度
    double actualHumidity;     // 箱内湿度
    uint8_t doorStatus;        // 箱门状态: 0-已关紧, 1-未关紧 (开门太久要报警)
    uint8_t compressorStatus;  // 压缩机状态: 0-待机, 1-制冷中
    uint8_t alarmCode;         // 故障码: 0-系统正常, 1-温度超限, 2-传感器脱落
};

// enum FuncCode : char{
//     FUNC_READ_TEMP = 0x03,//查询温度
//     FUNC_TEMP_DATA = 0x01,//温度返回
//     FUNC_SET_PARAM = 0x10 //下发参数
// };

// enum FuncCode : quint8 {
//     FUNC_HEARTBEAT       = 0x00,
//     // === 0x01 ~ 0x0F: 数据主动上报类 ===
//     FUNC_REPORT_ALL_DATA = 0x01,  // 下位机主动上报所有实时数据 (温湿度、状态、报警码)
//     FUNC_REPORT_ALARM    = 0x02,  // 下位机主动上报紧急故障 (可选：用于极速中断报警)

//     // === 0x10 ~ 0x1F: 上位机下发控制类 ===
//     FUNC_SET_PARAM       = 0x10,  // 上位机下发参数 (如修改温度阈值)
//     FUNC_CTRL_DEVICE     = 0x11,  // 上位机控制指令 (如远程消音、强制化霜)

//     // === 0x20 ~ 0x2F: 上位机主动查询类 ===
//     FUNC_READ_ALL_DATA   = 0x20,  // 上位机主动查询所有数据 (如上位机刚开机时同步状态)
//     FUNC_READ_PARAM      = 0x21   // 上位机查询当前设定参数
// };

const char FRAME_HEAD_1 = static_cast<char> (0xAA);//?const char and enum
const char FRAME_HEAD_2 = static_cast<char> (0x55);
const char FRAME_TAIL = static_cast<char> (0xFF);

const int PROTOCOL_MAX_DATALEN = 64;

struct Frame{
    quint8 funcCode;//
    QByteArray payload;
};

Q_DECLARE_METATYPE(DeviceData) //注册元类型
Q_DECLARE_METATYPE(Frame)

#endif // PROTOCOLDATA_H
