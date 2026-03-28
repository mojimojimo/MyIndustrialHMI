# 医疗级冷链设备监控系统

!\[C++](https://img.shields.io/badge/C++-11-blue.svg)
!\[Qt](https://img.shields.io/badge/Qt-6.5.3\_Widgets-41CD52.svg)
!\[CMake](https://img.shields.io/badge/CMake-Build\_System-red.svg)
!\[SQLite](https://img.shields.io/badge/SQLite-Database-003B57.svg)

## 📌 项目背景 | Background

在医疗领域，疫苗、血液及特定生物制品对储存环境的温湿度有着极度严苛的要求。温湿度超限可能导致生物制品失效，甚至引发医疗事故。
本项目是一个专为医疗冷链设备（如疫苗冷藏箱、血液冷藏柜）研发的 **PC 端 HMI（人机交互）系统**。系统向下支持双模通信（串口/TCP）采集设备数据，向上提供实时的状态监控、参数配置、异常告警拦截，并构建了基于 SQLite 的完整“监控-告警-追溯”数据闭环。

## 🏗️ 架构设计 | Architecture

系统采用**三层解耦与多线程架构**，彻底解决高频数据吞吐下的 UI 卡顿问题：

* **UI 渲染线程 (主线程)**：负责界面交互与 `QCustomPlot` 实时曲线绘制。采用**主动降采样抓取**策略，避免高频数据刷新阻塞事件循环。
* **通信与解析线程 (Worker Thread 1)**：封装 `CommWorker` 与 `ProtocolParser`。负责底层的 TCP/串口异步读写，并通过**环形缓冲区 (Ring Buffer)** 解决粘包、半包及脏数据问题。
* **持久化线程 (Worker Thread 2)**：独立负责 SQLite 数据库的慢速 I/O 操作。

*(业务数据流转模型：原始 Hex 字节流 ↔ Frame 结构 ↔ DeviceData/ConfigData 业务结构体)*

## ✨ 核心特性 | Key Features

* 🔌 **双模通信与容错机制**：支持 Serial Port 与 TCP Socket 热切换。内置看门狗（Watchdog）机制，实现通信超时检测与最大 5 次的自动断线重连。
* 📊 **实时监控与智能降流**：动态展示温湿度与设备状态（压缩机/箱门），温湿度曲线平滑无卡顿。
* ⚙️ **远程双向控制**：支持目标温湿度及上下限报警阈值的读取与一键下发；支持强制化霜、远程消音等硬件指令。
* 💾 **智能审计与数据追溯**：

  * **环境数据表 (`env\_history`)**：采用定时采样策略写入，大幅缩减无效 I/O。
  * **事件日志表 (`event\_logs`)**：采用状态突变检测策略，精准记录箱门开闭、系统报警等突发事件。
  * 支持基于时间区间的海量数据毫秒级检索，及一键导出医疗合规的 CSV 报表。
* 🛡️ **工程化与健壮性**：

  * 支持 INFO / WARNING / DEBUG / ERROR 四级日志追踪，支持原始 Hex 报文展示。
  * 基于 `QSettings` 的通信参数记忆化配置。
  * 采用 `QTest` 框架完成协议解析层面的全覆盖单元测试（覆盖脏数据、粘包、断包场景）。

## 📜 自定义通信协议 | Custom Protocol

底层采用轻量级、高可靠的自定义二进制协议进行交互：

|帧头 (2 Bytes)|功能码 (1 Byte)|数据长度 (1 Byte)|负载数据 (Payload)|校验和 (1 Byte)|帧尾 (1 Byte)|
|:-:|:-:|:-:|:-:|:-:|:-:|
|`0xAA 0x55`|`0x01` / `0x10` / ...|`0x00 \~ 0xFF`|`DeviceData` / `ConfigData`/ ...|`Sum Check`|`0xFF`|

## 📸 系统截图 | Screenshots

|实时监控大屏|参数配置与指令下发|
|:-:|:-:|
|![实时监控](https://github.com/mojimojimo/MyIndustrialHMI/blob/master/images/realtime.png?raw=true)|![参数配置](https://github.com/mojimojimo/MyIndustrialHMI/blob/master/images/config.png?raw=true)|
|**历史追溯与 CSV 导出**|**原始报文与分级日志**|
|![历史数据](https://github.com/mojimojimo/MyIndustrialHMI/blob/master/images/history.png?raw=true)|![分级日志](https://github.com/mojimojimo/MyIndustrialHMI/blob/master/images/logs.png?raw=true)|



