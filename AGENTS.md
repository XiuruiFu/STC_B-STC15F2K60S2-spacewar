# `AGENTS.md`

## 1. 项目概述 (Project Overview)
本项目旨在使用 **2 块 STB_B 嵌入式学习板**（搭载 STC15F2K60S2 芯片）与 **PC 端显示程序**，共同实现经典双人太空射击游戏 **《Spacewar!》** (1962)。下面是基础游戏功能的描述，实现基础功能后会拓展其它功能（如触发震动传感器后飞船加速等）。

### 逻辑拓扑与架构
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

1. **主机 (Host MCU / Player 1)**：
   - 采集本地按键/导航键作为 Player 1 输入。
   - 通过 **RS485 (Uart2)** 接收从机（Player 2）的按键状态帧。
   - 汇总双方控制指令，完成**全部逻辑计算**（物理引擎 + 游戏状态机），支持 `float`/`double` 运算（已验证）。
   - 将渲染所必需的数据（双方飞船坐标/朝向、子弹、黑洞、比分、状态机等）通过 **Micro-USB (Uart1)** 打包传输给 PC。
   - 本地数码管显示双方累计胜场，LED 显示 RS485 连接状态。
2. **从机 (Slave MCU / Player 2)**：
   - 作为独立手柄节点，采集本地按键/导航键输入。
   - 通过 **RS485 (Uart2)** **单向**将按键状态帧发送给主机（不接收任何下行帧）。
   - 本地 LED 显示连接状态。
3. **PC 客户端 (PC Renderer)**：
   - **仅负责画面渲染与音效播放**（物理引擎与状态机均由 Host 完成）。
   - 通过 USB 串口接收 Host 发送的游戏状态帧并绘制。
   - 技术栈：**Python + pygame**（串口用 pyserial）。

---

## 2. 硬件与底层库约束 (Hardware & BSP Constraints)

### 2.1 芯片与时钟
- **MCU**：STC15F2K60S2（8051 架构，小端序/51 存储模型）。
- **系统时钟 (`SysClock`)**：固化为 `11059200L` Hz (11.0592MHz)，全局须保持一致。

### 2.2 核心 API 与模块调用规范 (MySTC_B BSP Ver3.6b)
所有 C51 固件开发**必须**基于学院标准 BSP 库 (`STC_BSP.lib`, 源码在./inc中，**你在写代码前应该详细阅读头文件源码**)，严格遵循以下初始化与事件调度架构：

- **系统调度与时间基准 (`sys.h`)**：
  - `MySTC_Init()`：主入口初始化，必须首先调用。
  - `MySTC_OS()`：必须置于 `main()` 的 `while(1)` 循环中。
  - **非抢占式约束**：所有回调函数与主循环任务的单次执行时间**不得超过 1ms**。禁用任何阻塞型 `delay()`！
  - **事件响应**：统一使用 `SetEventCallBack(enumEvent, callback)` 绑定回调。
- **串口通信 1 (`uart1.h`)**：
  - 用于主机与 PC 端的 USB 通信，波特率设为 `115200` bps。
  - 必须使用非阻塞发送函数 `Uart1Print(pt, num)`。
- **串口通信 2 (`uart2.h`)**：
  - 用于两板之间的 RS485 总线通信（设置模式为 `Uart2Usedfor485`）。
  - 波特率固定为 **`38400` bps**。
  - 通过 `SetUart2Rxd(RxdPt, Nmax, matchhead, matchheadsize)` 设置数据包匹配帧头。
- **输入采集映射 (`adc.h` / `key.h`)**：
  - **在游戏进行中 (In-Game)**：
    - `Key 3` -> **向前推进** (Thrust Forward)
    - `Key 1` -> **向后推进** (Thrust Backward)
    - `Key 2` -> **发射/开火** (Fire)
    - `enumAdcNavKeyDown` (导航键下) -> **船头向左转** (Rotate Left)
    - `enumAdcNavKeyUp` (导航键上) -> **船头向右转** (Rotate Right)
  - **在菜单页面中 (In-Menu)**：
    - `enumAdcNavKeyLeft` / `enumAdcNavKeyRight` -> **上下选择菜单栏目**
    - `Key 2` -> **确定选择** (Confirm)
- **反馈显示与音效 (`displayer.h` / `beep.h`)**：
  - `Seg7Print()` 与 `LedPrint()` 用于指示状态。
  - `SetBeep()` 用于按键反馈（非阻塞）。
- **非易失存储 (`DS1302.h`)**：
  - 胜场数据通过 `NVM_Read()` / `NVM_Write()` 读写 DS1302 内部 NVM（地址 0~29 可用，地址 30 被系统占用）。
  - 需先 `DS1302Init(time)` 初始化。

- **BSP实现参考**：
    - 你可以在../sample_project中找到更多实现样例（含 `double测试` 浮点验证工程）。

### 2.3 项目目录结构
```text
project/
├── AGENTS.md               # 本文档
├── docs/
│   └── protocol.md         # 通信协议契约(三端唯一权威)
├── source/                 # Host MCU 工程源码
│   ├── main.c              # Host: 物理引擎+状态机+双串口+菜单
│   ├── sin_table.h         # 256 项 float 正弦查表(避免 8051 三角函数)
│   └── STCBSP_V3.6.LIB     # BSP 库
├── inc/                    # BSP 头文件(共享)
├── STC_Demo.uvproj         # Host Keil 工程
├── firmware_slave/         # Slave MCU 独立工程
│   ├── source/main.c       # Slave: 按键采集+RS485 上传
│   ├── inc/                # (复制自 inc/)
│   └── Slave.uvproj        # Slave Keil 工程
└── pc/                     # PC 渲染器(uv 管理)
    ├── main.py             # pygame 渲染 + pyserial
    └── pyproject.toml
```

- 编译：Host 与 Slave 分别在 Keil µVision 5 中打开对应 `.uvproj` 编译下载。
- PC 端：`uv sync` 后 `uv run python main.py --port COMx` 运行。

---

## 3. 代码生成与编码标准 (Coding Standards for Agents)

1. **C51 / Keil 兼容性**：
   - C 语言代码需严格遵守 ANSI C / Keil C51 标准，避免使用 C99 专有特性（如在函数中间声明变量）。
2. **通信协议安全性**：
   - RS485 及 USB 通信帧必须包含：`帧头(2Byte) + 数据 + 校验和(1Byte)`。
   - 接收缓冲区必须限制最大字节数，防止内存越界。
   - 校验和须在接收端验证，非法帧丢弃。

---

## 4. 需求规格 (Requirements Specification)

> 以下为已与用户达成共识的需求决策，实现时须严格遵守。

### 4.1 架构与职责分工
- **物理引擎 + 游戏状态机** 运行于 **Host MCU**（支持 float/double）。
- **Host↔Slave 单向通信**：Slave 仅发送按键帧，Host 不向 Slave 下发任何状态。
- **PC 仅渲染**：接收 Host 状态帧并绘制画面、播放音效。

### 4.2 游戏规则
- **生命制**：双方各 **3 条命**，被子弹命中或撞入黑洞扣 1 命，命耗尽判负，对方胜。
- **黑洞**：屏幕中央圆形，飞船/子弹撞入即被吞掉。**无引力场**。
- **子弹**：每船同时在场最多 **1 发**，有飞行寿命，命中对方扣其 1 命。
- **边界**：飞船/子弹飞出屏幕后**环绕到对侧**（toroidal 拓扑）。
- **运动物理**：双向推力 + 惯性滑行（Key3 前进加速、Key1 后退加速，旋转仅改变朝向，无阻力）。
- **死亡反馈与重生**：死亡时 Host 发 Beep 声、PC 显示爆炸特效；飞船延时 **1 秒**后于**随机位置重生**，并清空其在场子弹。

### 4.3 状态机与菜单
- 开机进入**主菜单**，含 4 个栏目（`enumAdcNavKeyLeft/Right` 选择，`Key 2` 确认）：
  1. **进入游戏** (Start Game)
  2. **双方总胜场** (Show Wins)
  3. **总胜场清零** (Clear Wins)
  4. **退出游戏** (Exit)
- 一局结束后**返回主菜单**。
- 双方累计胜场写入 **DS1302 NVM**（地址 0~29），掉电保持。

### 4.4 数据表示与节拍
- **坐标**：0~255 整数（Host 内部 float 积分，发送时量化到 0~255）；PC 按屏幕比例缩放。
- **朝向角**：1 字节 0~255，表示 0~360°（分辨率 360/256 ≈ 1.41°）。
- **节拍**：物理积分与 Host→PC 状态帧均采用 **10ms 时间步（100Hz）**（基于 `enumEventSys10mS`）。

### 4.5 通信协议
- **RS485 (Uart2, Slave→Host)**：Slave 采集的 P2 按键掩码帧。
  - 格式：`帧头 2B + 按键掩码 1B + 校验和 1B`。
- **USB (Uart1, Host→PC)**：Host 打包的游戏状态帧。
  - 格式：`帧头 2B + 状态数据(双方坐标/朝向、子弹、黑洞、比分、状态机等) + 校验和 1B`。
- 波特率：Uart1 `115200`，Uart2 `38400`。

### 4.6 本地显示
- **Host 数码管**：显示双方累计胜场。
- **LED**：指示 RS485 连接状态（Slave 同理）。

### 4.7 范围
- 本次实现**仅基础功能**；震动传感器加速等扩展功能另行规划（见扩展章节）。

---

## 5. 扩展功能规划 (Optional Extensions)

- 触发震动传感器（`enumEventVib`）后飞船短暂加速。
- 其它可扩展玩法（如黑洞引力、多弹、道具等）。
