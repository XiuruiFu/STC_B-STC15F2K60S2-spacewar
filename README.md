# Spacewar! —— 双板联机手柄 + PC 渲染系统

基于 **2 块 STB_B 嵌入式学习板**（STC15F2K60S2, 8051）与 **PC 端渲染程序**，复刻并扩展了 1962 年世界上第一款电子游戏 **《Spacewar!》**。全部游戏逻辑（物理引擎 + 状态机）运行在主机单片机上，PC 仅负责画面渲染与音效。

---

## 1. 系统架构

```text
         +------------------+
         |  Slave Board     |
         | (Player 2 手柄)  |
         +--------+---------+
                  |  RS485 (Uart2, 38400 bps)  只上行按键帧
                  |
         +--------+---------+
         |  Host Board      |
         | (Player 1 &      |
         |  物理引擎+状态机) |
         +--------+---------+
                  | Micro-USB (Uart1, 115200 bps)  下行状态帧
                  v
         +------------------+
         |  PC Client       |
         | (画面渲染/显示)  |
         +------------------+
```

| 角色 | 职责 |
|------|------|
| **Host (Player 1)** | 采集本地按键/摇杆；经 RS485 接收 P2 按键帧；完成**全部物理与状态机运算**（支持 `float`）；经 USB 向 PC 下发状态帧；数码管显示累计胜场，LED 显示连接状态 |
| **Slave (Player 2)** | 独立手柄节点，采集按键/摇杆/霍尔；经 RS485 **单向**上传按键帧（含护盾标志）；本地 LED/数码管指示；循环播放背景音乐 |
| **PC Renderer** | Python + pygame，解析状态帧绘制画面、播放音效，**不做任何游戏逻辑** |

---

## 2. 功能特性

- **基础对战 (PvP)**：双人各 3 条命，双向推力 + 惯性滑行 + 环绕边界（toroidal）；中央黑洞吞噬飞船/子弹；每船最多 `BULLET_MAX` 发子弹；死亡 1 秒后随机位置重生（避开黑洞）。
- **菜单与持久化**：主菜单 3 项（进入游戏 / 总胜场清零 / 退出）；累计胜场写入 DS1302 NVM，掉电保持。
- **昼夜背景 (E2)**：开始游戏瞬间读取光敏电阻 `Rop`，`> 阈值` 判为白天（白洞斥力）否则夜晚（黑洞引力）；PC 按 `bgMode` 加载 `day.jpg` / `night.jpg`。
- **引力系统**：平方反比 + 距离下限截断，仅作用于飞船，白洞斥力 / 黑洞引力随昼夜切换。
- **多弹与配置**：子弹数、存活时间等全部集中在 `source/config.h` 与 `pc/config.py`，协议帧长由宏自动推导。
- **温度护盾**：手按温度传感器至 >30°C，船头前方出现 **75° 扇形护盾**，挡掉敌方/BOSS 子弹；双方各判本板温度。
- **彩蛋合作打 BOSS (PvE)**：从机霍尔 → 红外 → 主机接收后武装，进入彩蛋地图（纯白背景）双人合力击毁中央旋转 BOSS（周期散射弹幕 + 命中反击），共同胜负。
- **从机背景音乐**：从机循环播放硬编码背景乐（`music.h`）；所有按键/游戏音效统一由主机蜂鸣器发声（单音轨分工）。

---

## 3. 硬件模块

主机与从机均使用学院标准 **STB_B 学习板** BSP 库 (`STCBSP_V3.6.LIB`)，系统时钟 11.0592MHz。项目实际使用的外设：

| 模块 | 用途 |
|------|------|
| 5 向导航摇杆 + K3 | 旋转 / 前进 / 菜单选择 |
| 独立按键 Key1 / Key2 | 后退 / 开火·确认 |
| 8 位数码管 Seg7 | 显示双方累计胜场 / Player 编号 |
| 8 位 LED | RS485 连接状态、菜单项、彩蛋流水灯 |
| 蜂鸣器 Beep | 按键与游戏音效（主机） |
| NTC 热敏电阻 `Rt` (10K/3950) | 温度护盾触发 |
| 光敏电阻 `Rop` (GL5516) | 昼夜背景判定 |
| RS485 (Uart2) | 双板按键帧通信 |
| USB-UART (Uart1) | 主机↔PC 状态帧通信 |
| DS1302 RTC/NVM | 胜场持久化 |
| 霍尔传感器 | 彩蛋触发（磁场靠近/离开） |
| 红外发射/接收 | 彩蛋魔数无线传递 |
| `music.h` 音乐模块 | 从机循环背景音乐 |

---

## 4. 项目目录结构

```text
project/
├── AGENTS.md                  # 开发协作说明（架构、BSP 约束、调试经验、需求规格）
├── README.md                  # 本文件
├── docs/
│   └── protocol.md            # 三端通信协议契约（唯一权威）
├── inc/                       # BSP 头文件（Host 工程 include 路径）
│   ├── sys.H / adc.h / Key.H / uart1.h / uart2.h
│   ├── displayer.h / Beep.h / DS1302.h / IR.h / hall.H / music.h
│   ├── temp.h                 # NTC ADC→温度换算（Host 副本）
│   └── ...
├── source/                    # Host 固件工程（P1 + 物理引擎 + 状态机）
│   ├── main.c                 # 物理引擎 + 状态机 + 双串口 + 菜单 + 彩蛋
│   ├── config.h               # 统一可调配置（物理常量 / 阈值 / BOSS / 护盾）
│   ├── sin_table.h            # 256 项 float 正弦查表（避免 8051 三角函数）
│   └── STCBSP_V3.6.LIB
├── STC_Demo.uvproj            # Host Keil µVision 工程
├── firmware_slave/            # Slave 固件工程（P2 手柄）
│   ├── source/main.c          # 按键采集 + RS485 上传 + 背景音乐 + 温度护盾
│   ├── inc/                   # BSP 头文件副本（含 temp.h）
│   └── Slave.uvproj           # Slave Keil µVision 工程
└── pc/                        # PC 渲染器（Python 3.12 + uv 管理）
    ├── main.py                # pygame 渲染 + pyserial 帧同步
    ├── config.py              # 渲染参数 / 颜色主题 / 协议联动常量
    ├── pyproject.toml
    ├── uv.lock
    └── assets/                # day.jpg / night.jpg / boss.jpg
```

> 编译产物（`output/`、`list/`、`*.uvopt`、`*.hex` 等）及报告相关文档不入库，见 `.gitignore`。

---

## 5. 快速开始

### 5.1 Host / Slave 固件（Keil µVision 5）

1. Host：打开根目录 `STC_Demo.uvproj`，编译并下载到 **主机板**。
2. Slave：打开 `firmware_slave/Slave.uvproj`，编译并下载到 **从机板**。
3. 两板通过 RS485 接口连接，主机经 Micro-USB 连接 PC。

> 必须使用**正版授权 Keil**；评估版链接器限制 `code ≤ 2KB`，本工程浮点库即超限（报错 `FATAL ERROR L250`）。

### 5.2 PC 渲染器

```bash
cd pc
uv sync                                   # 安装依赖 (pygame / pyserial)

uv run python main.py                     # 自动选择第一个可用串口
uv run python main.py --port COM3         # 指定串口
uv run python main.py --list              # 列出可用串口
uv run python main.py --sim               # 无硬件模拟模式
```

---

## 6. 操作说明

**游戏中**

| 输入 | 动作 |
|------|------|
| Key3 | 向前推进 |
| Key1 | 向后推进 |
| Key2 | 发射 |
| 导航键 下 / 上 | 船头 左转 / 右转 |

**菜单中**

| 输入 | 动作 |
|------|------|
| 导航键 左 / 右 | 上下选择菜单项 |
| Key2 | 确认 |

菜单项：`0` 进入游戏、`1` 总胜场清零、`2` 退出游戏。

---

## 7. 通信协议

三端唯一权威契约见 **[`docs/protocol.md`](docs/protocol.md)**。概要：

| 链路 | 接口 | 波特率 | 方向 | 帧长 |
|------|------|--------|------|------|
| Host → PC | Uart1 (Micro-USB) | 115200 | 下行 | `20 + 6*BULLET_MAX + 3*BOSS_BULLET_MAX` 字节 |
| Slave → Host | Uart2 (RS485) | 38400 | 上行 | 5 字节（帧头2 + 按键1 + 护盾1 + 校验1） |

- 所有帧均为 `帧头(2B) + 数据 + 校验和(1B)`，校验和为帧内其余字节之和的低 8 位。
- Host→PC 状态帧包含双方飞船、玩家子弹、胜场、flags、昼夜 `bgMode`、彩蛋 `easterEgg`、护盾 `shield`、BOSS 血量与 BOSS 子弹。
- 帧长与各字段偏移全部由 `BULLET_MAX` / `BOSS_BULLET_MAX` 宏推导，**禁止写死偏移**。

---

## 8. 关键技术点

- **正弦查表**：`sin_table.h` 提供 256 项 `float` 查表，8051 免三角函数实现朝向/推力方向。
- **定步长物理**：`enumEventSys10mS` 回调内以 100Hz 时间步积分，回调单次执行 < 1ms（非抢占式）。
- **非阻塞通信**：`Uart1Print` / `Uart2Print` 异步发送，发送缓冲须为全局 `xdata` 数组。
- **统一配置**：`source/config.h`（固件）与 `pc/config.py`（渲染）集中可调参数，改一处即可；`BULLET_MAX` 等会联动协议帧长。
- **NTC 温度换算**：`inc/temp.h` 查表 + 线性插值，将 10bit ADC 换算为 0.1°C。
- **PC 帧同步**：非阻塞读尽串口缓冲、只取最新一帧渲染，避免帧堆积导致显示延迟。

更多底层陷阱与调试经验（Key3 必须用 ADC 读、`AdcInit(ADCexpEXT)`、异步发送缓冲等）见 **[`AGENTS.md`](AGENTS.md)** 第 4 章。

---

## 9. 相关文档

- [`docs/protocol.md`](docs/protocol.md) —— 三端通信协议（权威契约）
- [`AGENTS.md`](AGENTS.md) —— 架构、BSP 约束、开发调试经验、需求规格与扩展规划
- [`pc/README.md`](pc/README.md) —— PC 渲染器运行说明
