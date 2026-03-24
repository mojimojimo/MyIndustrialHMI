cmake_minimum_required(VERSION 3.16)

project(MyIndustrialHMI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort PrintSupport Network Sql Test) #串口模块
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort PrintSupport Network Sql Test)

set(PROJECT_SOURCES
        src/main.cpp

        #UI
        src/ui/mainwindow.cpp
        src/ui/mainwindow.h
        src/ui/mainwindow.ui
        src/ui/historywidget.h
        src/ui/historywidget.cpp
        src/ui/historywidget.ui

        #协议解析
        src/protocol/ProtocolData.h
        src/protocol/protocolparser.h
        src/protocol/protocolparser.cpp

        # 设备管理
        src/device/devicemanager.cpp
        src/device/devicemanager.h

        # 通信
        src/comm/commworker.cpp
        src/comm/commworker.h

        src/comm/serialworker.cpp
        src/comm/serialworker.h

        src/comm/tcpworker.cpp
        src/comm/tcpworker.h

        # 数据库
        src/database/databasemanager.cpp
        src/database/databasemanager.h

        # 第三方库
        src/thirdparty/qcustomplot.cpp
        src/thirdparty/qcustomplot.h
)

# if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
#     qt_add_executable(MyIndustrialHMI
#         MANUAL_FINALIZATION
#         ${PROJECT_SOURCES}
#         serialworker.h serialworker.cpp
#         qcustomplot.cpp qcustomplot.h
#         ProtocolData.h
#         protocolparser.h protocolparser.cpp
#         devicemanager.h devicemanager.cpp
#         commworker.h commworker.cpp
#         tcpworker.h tcpworker.cpp
#         databasemanager.h databasemanager.cpp
#         historydialog.h historydialog.cpp historydialog.ui

#     )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET MyIndustrialHMI APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
# else()
#     if(ANDROID)
#         add_library(MyIndustrialHMI SHARED
#             ${PROJECT_SOURCES}
#         )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
   # else()
        add_executable(MyIndustrialHMI
            ${PROJECT_SOURCES}
        )
#     endif()
# endif()

#头文件搜索路径
target_include_directories(MyIndustrialHMI PRIVATE

   src
   src/ui
   src/protocol
   src/device
   src/comm
   src/database
   src/thirdparty
)

target_link_libraries(MyIndustrialHMI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::SerialPort Qt${QT_VERSION_MAJOR}::PrintSupport
 Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Test)


# 启用单元测试
enable_testing()
# 添加tests子目录
add_subdirectory(tests)


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MyIndustrialHMI)
endif()
set_target_properties(MyIndustrialHMI PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MyIndustrialHMI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MyIndustrialHMI)
endif()
