#ifndef PROTOCOLDATA_H
#define PROTOCOLDATA_H

enum FuncCode : char{
    FUNC_READ_TEMP = 0x03,//查询温度
    FUNC_TEMP_DATA = 0x01,//温度返回
    FUNC_SET_PARAM = 0x10 //下发参数
};

const char FRAME_HEAD_1 = static_cast<char> (0xAA);//?const char and enum
const char FRAME_HEAD_2 = static_cast<char> (0x55);
const char FRAME_TAIL = static_cast<char> (0xFF);

const int PROTOCOL_MAX_DATALEN = 64;

struct Frame{
    quint8 funcCode;//
    QByteArray payload;
};
//Q_DECLARE_METATYPE(Frame) 注册元类型

#endif // PROTOCOLDATA_H
