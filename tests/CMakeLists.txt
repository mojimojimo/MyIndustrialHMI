cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(tst_protocolparser

    tst_protocolparser.cpp
    ../src/protocol/protocolparser.cpp
)

target_include_directories(tst_protocolparser PRIVATE

    ../src
    ../src/protocol
)

target_link_libraries(tst_protocolparser PRIVATE

    Qt6::Test
)

# 注册测试
add_test(
    NAME ProtocolParserTest
    COMMAND tst_protocolparser
)
