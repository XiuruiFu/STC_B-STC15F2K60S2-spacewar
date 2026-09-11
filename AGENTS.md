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
   - **仅负责画面渲染**（物理引擎与状态机均由 Host 完成）。
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
├── README.md               # 项目说明
├── docs/
│   └── protocol.md         # 通信协议契约(三端唯一权威)
├── source/                 # Host MCU 工程源码
│   ├── main.c              # Host: 物理引擎+状态机+双串口+菜单+彩蛋+BOSS+护盾
│   ├── config.h            # 统一可调配置(物理常量/阈值/BOSS/护盾)
│   ├── sin_table.h         # 256 项 float 正弦查表(避免 8051 三角函数)
│   └── STCBSP_V3.6.LIB     # BSP 库
├── inc/                    # BSP 头文件(Host include 路径, 共享)
│   ├── sys.H / adc.h / Key.H / uart1.h / uart2.h
│   ├── displayer.h / Beep.h / DS1302.h / IR.h / hall.H / music.h ...
│   └── temp.h              # NTC ADC→温度换算(Host 副本)
├── STC_Demo.uvproj         # Host Keil 工程
├── firmware_slave/         # Slave MCU 独立工程
│   ├── source/main.c       # Slave: 按键采集+RS485 上传+背景音乐+温度护盾
│   ├── inc/                # (复制自 inc/, 含 temp.h)
│   └── Slave.uvproj        # Slave Keil 工程
└── pc/                     # PC 渲染器(uv 管理)
    ├── main.py             # pygame 渲染 + pyserial
    ├── config.py           # 渲染参数/颜色主题(与 Host config.h 联动)
    ├── pyproject.toml
    ├── uv.lock
    └── assets/             # day.jpg / night.jpg / boss.jpg
```

- 编译：Host 与 Slave 分别在 Keil µVision 5 中打开对应 `.uvproj` 编译下载。
- PC 端：`uv sync` 后 `uv run python main.py --port COMx` 运行。
- 编译产物（`output/`、`list/`、`*.uvopt`、`*.hex` 等）与报告文档不入库，见 `.gitignore`。

---

## 3. 代码生成与编码标准 (Coding Standards for Agents)

1. **C51 / Keil 兼容性**：
   - C 语言代码需严格遵守 ANSI C / Keil C51 标准，避免使用 C99 专有特性（如在函数中间声明变量）。
2. **通信协议安全性**：
   - RS485 及 USB 通信帧必须包含：`帧头(2Byte) + 数据 + 校验和(1Byte)`。
   - 接收缓冲区必须限制最大字节数，防止内存越界。
   - 校验和须在接收端验证，非法帧丢弃。

---

## 4. 开发与调试经验 (Hard-Won Lessons)

> 以下为本项目联调过程中总结的**关键陷阱**，实现/修改固件时必须遵守。违反这些会导致难以排查的隐性 bug。

1. **Keil 授权**：Keil µVision 必须使用**正版/已授权**版本。评估版 (Eval Version) 链接器限制 code ≤ 2KB，无法编译本工程（浮点库即超限）。报错特征：`FATAL ERROR L250: CODE SIZE LIMIT IN RESTRICTED VERSION EXCEEDED`。

2. **Key3 (前进) 不能用 `GetKeyAct(enumKey3)`**：Key3 与导航摇杆的 K3 共用 P1.7 端口。BSP 规定 P1.7 上的 Key3 **必须**用 `GetAdcNavAct(enumAdcNavKey3)` 读取（触发 `enumEventNav` 事件）。`GetKeyAct(enumKey3)` 永远读不到，前进键会完全失效。Host 与 Slave 两端都要遵守。

3. **导航键/摇杆必须 `AdcInit(ADCexpEXT)`**：凡需用 `GetAdcNavAct()` 检测导航键（上/下/左/右/K3），`AdcInit()` 参数必须是 `ADCexpEXT`。用 `ADCincEXT` 会导致导航键事件完全不触发（症状：拨动摇杆无反应）。`ADCexpEXT` 会占用 EXT 接口的 P1.0/P1.1 作 ADC。

4. **UART 异步发送缓冲区必须用全局 `xdata` 数组**：`Uart1Print`/`Uart2Print` 是异步的（调用返回约 1µs，后台继续发）。若传入**函数内局部栈数组**，函数返回后该内存被其他函数的局部变量覆盖（C51 对 data 区做 Overlay 复用），导致帧头/数据被破坏——典型症状：帧头 `0xAA 0x55` 收到 `0xAA 0x00`（0x55 被覆盖）。**所有发送缓冲必须声明为全局 `xdata` 数组**（Host 的 `uart1_tx[FRAME_TX_LEN]`、Slave 的 `uart2_tx[5]` 已如此）。

5. **`enumEventNav` 回调里要轮询所有关心的导航键**：`GetAdcNavAct()` 每次只返回一个键的事件（查询一次后该键事件清零）。在 `cb_nav` 里应逐个调用，不要只查一个。

6. **P2 按键边沿检测**：Host 在 `cb_10ms` 里用 `(p2_keys & BIT) && !(p2_keys_prev & BIT)` 检测 P2 的边沿（开火/菜单确认）。**同一 tick 内不能对同一边沿做两次消费**——若先 `menu_confirm()` 切到新状态，又在同一函数体后续分支再次检测同一边沿，会立刻被改回。`p2_keys_prev = p2_keys` 更新须放在所有边沿检测之后。

7. **PC 串口帧同步**：PC 端 `read_frame` 用 `timeout=0` 非阻塞读取，`while` 循环读尽缓冲、只取最新一帧渲染。若每 tick 只消费一帧或阻塞读，帧堆积会导致显示延迟数秒。

8. **显示器初始化**：`DisplayerInit()` 后须手动 `Seg7Print(10,10,...)`（全灭）+ `LedPrint(0)`（灭灯）做初始清屏。

9. **`BULLET_MAX` 与协议帧长必须联动，禁止写死偏移**：Host→PC 状态帧的子弹区长度、胜场/flags/bgMode/easterEgg/shield/校验和偏移、`FRAME_TX_LEN` 必须全部由 `BULLET_MAX`/`BOSS_BULLET_MAX` 用宏推导（当前 `FRAME_TX_LEN = 20 + 6*BULLET_MAX + 3*BOSS_BULLET_MAX`），PC 端 `FRAME_LEN` 同理由 `config.BULLET_MAX`/`config.BOSS_BULLET_MAX` 推导。若写死 `uart1_tx[30/31/32]` 等偏移而 `BULLET_MAX` 变大，子弹区会与后续字段重叠、缓冲区越界，症状为“Host 子弹射出瞬间消失、Slave 子弹显示错乱”。改 `BULLET_MAX` 时须**同时**改 `source/config.h` 与 `pc/config.py` 两处保持一致。

10. **`BOSS_BULLET_MAX` 同样须与协议帧长联动**：合作战 BOSS 子弹区追加在 `easterEgg`/`shield`/`bossHp` 之后，`FRAME_TX_LEN = 20 + 6*BULLET_MAX + 3*BOSS_BULLET_MAX`；PC 端 `FRAME_LEN` 同理由 `config.BULLET_MAX` 与 `config.BOSS_BULLET_MAX` 推导。改动任一子弹上限，两端都必须同步，否则帧错位。

---

## 5. 需求规格 (Requirements Specification)

> 以下为已与用户达成共识的需求决策，实现时须严格遵守。

### 4.1 架构与职责分工
- **物理引擎 + 游戏状态机** 运行于 **Host MCU**（支持 float/double）。
- **Host↔Slave 单向通信**：Slave 仅发送按键帧，Host 不向 Slave 下发任何状态。
- **PC 仅渲染**：接收 Host 状态帧并绘制画面、播放音效。

### 4.2 游戏规则
- **生命制**：双方各 **3 条命**，被子弹命中或撞入黑洞扣 1 命，命耗尽判负，对方胜。
- **黑洞**：屏幕中央圆形，飞船/子弹撞入即被吞掉。有引力场。
- **子弹**：每船同时在场最多 **4 发**，有飞行寿命，命中对方扣其 1 命。
- **边界**：飞船/子弹飞出屏幕后**环绕到对侧**。
- **运动物理**：双向推力 + 惯性滑行（Key3 前进加速、Key1 后退加速，旋转仅改变朝向，无阻力）。
- **死亡反馈与重生**：死亡时 Host 发 Beep 声、PC 显示爆炸特效；飞船延时 **1 秒**后于**随机位置重生**，并清空其在场子弹。

### 4.3 状态机与菜单
- 开机进入**主菜单**，含 3 个栏目（`enumAdcNavKeyLeft/Right` 选择，`Key 2` 确认；P2 通过 RS485 按键帧同样可操作）：
  1. **进入游戏** (Start Game)
  2. **总胜场清零** (Clear Wins)
  3. **退出游戏** (Exit)
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

## 6. 当前实现状态 (Implementation Status)

> 基础功能已完成验收。以下为当前代码事实与已知边界，供后续开发参考。

- **已完成**：Host 物理引擎（双向推力+惯性+环绕+黑洞+子弹+碰撞+胜负）、状态机（菜单 3 项/游戏/结束/退出）、双串口、胜场持久化（DS1302 NVM 地址 0~2）、Slave 按键采集与上传、PC pygame 渲染器。
- **菜单项**（3 项）：`0` 进入游戏、`1` 总胜场清零、`2` 退出游戏。
- **Exit 行为**：Host 进入 `ST_EXITED` 后延时 1 秒（`exit_tick=100`）自动回菜单；PC 检测到 `state==ST_EXITED` 直接关闭窗口退出程序（**无 EXITED 画面**）。
- **状态机常量**：`ST_MENU=0`、`ST_PLAYING=1`、`ST_GAMEOVER=2`、`ST_EXITED=3`。
- **按键掩码位**（Host 与 Slave 一致）：`0x01` 前进(Key3)、`0x02` 后退(Key1)、`0x04` 发射/确认(Key2)、`0x08` 左转(NavDown)、`0x10` 右转(NavUp)、`0x20` 菜单上移(NavLeft)、`0x40` 菜单下移(NavRight)。
- **协议帧**：Slave→Host 5 字节（含护盾标志）；Host→PC 可变长度 `20 + 6*BULLET_MAX + 3*BOSS_BULLET_MAX` 字节（BULLET_MAX 为每船子弹上限，BOSS_BULLET_MAX 为 BOSS 子弹上限；Host `config.h` 与 PC `config.py` 必须一致）。详见 `docs/protocol.md`（三端唯一权威契约）。
- **可调物理常量**（已迁入 `source/config.h`）：`THRUST=0.05f`（推力）、`ROT_SPEED=2`（转速）、`MAX_SPEED=2.0f`（限速）、`BULLET_SPEED=2.0f`（子弹速度）、`BULLET_LIFE`（子弹寿命 ticks）、`BULLET_MAX`（每船子弹上限）、`LIVES_MAX=3`（生命）、`BH_R=12`（黑洞半径）、`FIELD=256`（场域）、`RESPAWN_TICKS=100`（重生延时）、`LIGHT_THRESHOLD=30`（光敏阈值）、`GRAVITY`/`GRAVITY_MIN_R`（引力系统）、`BOSS_HP=15`/`BOSS_BULLET_*`（合作战 BOSS）、`SHIELD_R=15`/`SHIELD_TEMP=300`（温度护盾，张角 75°）。
- **昼夜背景**（E2 已实现）：状态帧含 `bgMode` 字节（0=夜晚、1=白天）；`bgMode` 在"开始游戏"瞬间由 `GetADC().Rop > LIGHT_THRESHOLD` 判定，游戏过程中锁定不变。
- **合作打 BOSS**（7.4 已实现）：`easterEgg=1` 时进入纯合作 PvE，中央 BOSS（固定、无引力、圆碰撞 `BH_R`、周期旋转散射）取代黑洞；双方合力击毁 BOSS（`bossHp` 归零）共同胜利，双方都出局则失败；友伤关闭，不写 NVM 胜场。
- **从机背景音乐 + 音效分工**：从机（Slave）仅播放**循环背景音乐**（`music.h` 硬编码乐谱，`cb_led` 里 `GetPlayerMode()==enumModeStop` 时 `SetPlayerMode(enumModePlay)` 自动重播）；所有按键/游戏音效（开火、菜单确认、死亡、胜负等）统一由**主机（Host）`SetBeep`** 发声（蜂鸣器单音轨，故从机不再 `SetBeep` 以免与 BGM 抢音轨）。

---

## 7. 扩展功能规划 (Optional Extensions)

> 以下为待实现/进行中的扩展功能需求。分章节记录，每章之间相互独立。

### 7.1 背景图片化 (Background Image)
- 将当前纯色背景替换为**两张背景图片**：白天、夜晚各一张，由 PC 按 `bgMode` 选择。
- **格式与路径**：JPG 格式，存放于 `pc/assets/day.jpg` 与 `pc/assets/night.jpg`。
- **尺寸与映射**：任意分辨率，由 pygame 缩放铺满窗口（`WINDOW_WIDTH × WINDOW_HEIGHT`），供图时无需关心分辨率。
- **缺图兜底**：图片缺失或加载失败时，回退到现有纯色背景 + 配色主题（当前 `config.py` 的 `COLOR_BG_GAME`/`COLOR_BG_GAME_DAY`），程序**不得崩溃**。
- **背景图不含洞**：黑洞/白洞仍由 PC 按 `BH_R` 在背景之上**叠加绘制**（见 7.2），确保绘图与实际碰撞半径精确对齐。
- 位于 PC 端，Host 固件与协议**无改动**（复用现有 `bgMode`）。

### 7.2 引力系统 (Gravity System)
- 场域中心存在一个"洞"，其**类型由 `bgMode` 决定**：
  - 白天（`bgMode=1`）→ **白洞**，制造**斥力**（推离中心）；
  - 夜晚（`bgMode=0`）→ **黑洞**，制造**引力**（拉向中心）。
- 洞类型在"开始游戏"瞬间随 `bgMode` 一并确定，游戏过程中锁定不变（光照变化不影响）。
- **作用范围**：全场生效，无作用半径上限；仅设**距离下限截断** `GRAVITY_MIN_R`，防止 r→0 时数值发散（实际飞船会在 `r < BH_R+SHIP_R` 时死亡，截断仅作兜底）。
- **力学模型**：**平方反比 + 距离下限截断**。力大小 `F = GRAVITY / max(r², GRAVITY_MIN_R²)`；方向：斥力取"洞→飞船"单位向量（推离洞），引力取"飞船→洞"单位向量（拉向洞）。加速度作用在每 10ms tick 的物理积分中，并受现有 `MAX_SPEED` 上限钳制。
- **作用对象**：引力/斥力**仅影响飞船**，**不影响子弹**。
- **碰撞击杀**：无论黑白，飞船越过洞表面边界（`BH_R + SHIP_R`）即死亡（沿用现有 `kill_ship` 碰撞判定）。白洞斥力不会大到使飞船完全无法接近。
- **子弹**：撞入洞（`BH_R + BULLET_R`）**照样消失**（与现有黑洞吞子弹一致，黑白对称）。
- **可调常量**（迁入 `config.h`）：`GRAVITY=3.0`（引力常数，每 tick 加速度）、`GRAVITY_MIN_R=6`（距离下限截断）。初始值待实机标定手感。
- **实现要点**：洞在中心 `(128,128)`，任意点到中心的单轴距离 ≤ `FIELD/2`，故 `dist2_wrap` 的环绕分支对中心洞**永不生效**；引力方向直接取普通欧氏方向即可（中心到洞无需环绕）。
- **PC 端绘制**：黑洞 = 黑圆 + 紫环（现有样式）；白洞 = 白圆 + 浅色环（新增白洞样式，配色进 `config.py`）。

### 7.3 彩蛋模式 (Easter Egg)
- **触发链路**：从机霍尔传感器 → 从机红外发射 → 主机红外接收（两板红外接口物理对齐时才能收到，罕见故为彩蛋）。该链路**独立于 RS485 按键通道**。
- **从机**：
  - `HallInit()`，绑定 `enumEventHall` 回调；`GetHallAct()` 返回 `enumHallGetClose`（磁场靠近）或 `enumHallGetAway`（离开），**任一即触发**。
  - `IrInit(NEC_R05d)`；触发后 `IrPrint(&magic, 1)` 发送 **1 字节魔数 `0xE1`**。
  - **不防抖**；无需本地反馈。
- **主机**：
  - `IrInit(NEC_R05d)`，`SetIrRxd(ir_rx, 1)`；`enumEventIrRxd` 事件收到包后校验 `ir_rx[0]==0xE1` → 置 `g_easterArmed=1` 并 `SetBeep` 提示（仅 0→1 转变时响）。
  - **无解除武装设计**；标志**开局即消耗**：`menu_confirm` 进入游戏时若 `g_easterArmed`，本局进入彩蛋地图并清零标志（一次性）。
- **彩蛋地图（本局）**：
  - PC 背景**纯白**（`(255,255,255)`）。
  - 中央为 **BOSS**（取代黑洞、**无引力**），具体玩法见 7.4；**无视亮度**（`easterEgg=1` 时 `bgMode` 仅用于飞船/HUD 主题）。
  - 飞船/子弹/HUD 沿用**白天主题配色**（深色，纯白背景上可见）。
- **主机 LED**：处于 `ST_MENU` 且 `g_easterArmed` 时，LED 持续**流水灯**（l0→l1→…→l7→l0 循环，参考"八位数码管+流水灯"样例 `a=(a==0)?1:(a<<1)`）；进入游戏后恢复常规 LED。
- **协议**：状态帧新增 1 字节 `easterEgg`（0/1），位于 `bgMode` 之后；PC 解析该字节并据此渲染（`easterEgg=1` 时忽略 `bgMode` 绘制纯白+BOSS）。最终帧长见 7.5 与 `docs/protocol.md`（`FRAME_TX_LEN = 20 + 6*BULLET_MAX + 3*BOSS_BULLET_MAX`）。

### 7.4 彩蛋内容 —— 双人合作打 BOSS (Co-op Boss Battle)

> 彩蛋地图（`easterEgg=1`）的**具体游戏内容**。已与用户对齐，实现时严格遵守。

- **总体**：`easterEgg=1` 时进入**纯合作 PvE** 模式，双方合力击毁中央 **BOSS**，共同的胜利/失败。此时**不再是 PvP 对抗**。
- **BOSS 取代黑洞**：
  - 取消中央黑洞（不再吞飞船/吞子弹），**无引力**（`GRAVITY` 系统在本模式不生效）。
  - BOSS **固定于中央** `(128,128)`，不移动。
  - 碰撞体积保持**圆形**，半径沿用 `BH_R=12`（与原黑洞相同）。
  - 飞船碰到 BOSS 即死（沿用 `BH_R + SHIP_R` 碰撞判定）；玩家子弹命中 BOSS 扣血并消失（`BH_R + BULLET_R` 判定）。
- **BOSS 血量与胜负**：
  - BOSS **血量 `15`**，每发玩家子弹命中扣 1 血。
  - **胜利**（BOSS 血归零）：双方**共同胜利**；BOSS 爆炸特效 + 蜂鸣，PC 显示 `BOSS DEFEATED!`，中央爆炸，场上残留 BOSS 子弹全部清除，2 秒后回菜单。
  - **失败**：双方都出局才结束（各自 3 条命、死亡 1 秒后重生；单方出局比赛继续）；PC 显示 `CO-OP FAILED`，2 秒后回菜单。
  - 合作战**不写入 NVM 累计胜场**（`p1wins/p2wins` 不变）。
- **BOSS 射击**：**周期旋转散射**——每 **3 秒**（300 ticks）朝四周均布射 **8 发**，每轮整体角度**旋转 `15°`**，形成螺旋弹幕。
- **BOSS 反击技能**：BOSS 每被玩家子弹命中 1 发（扣 1 血的同时），**朝发射该子弹的飞船当前位置**反击 1 发（`boss_retaliate`，按"洞→飞船"方向归一化向量，速度/寿命同 BOSS 子弹）。反击弹**复用同一 BOSS 子弹池**（无空位则此次不发射）。
- **玩家子弹规则（合作模式）**：
  - **友伤关闭**：玩家子弹互不伤害（取消 `bullet1↔ship2`、`bullet2↔ship1` 命中判定）。
  - 玩家子弹只对 BOSS 生效；玩家子弹与 BOSS 子弹**互不碰撞**（各自穿过）。
- **BOSS 子弹参数**（迁入 `config.h`）：
  - 速度 `Boss 子弹速度 = 1.2f`、碰撞半径 `3`、同屏上限 `12`（散射 8 + 反击占用同一池）、**寿命 `120`**（10ms ticks）、每轮散射步进 `15°`（256 制 ~ 10~11 单位）。
- **BOSS 贴图与旋转**：
  - 图片 `pc/assets/boss.jpg`（已就位，近正方形，内切圆为 BOSS 圆，四角透明）。
  - 旋转由 **PC 端按本地时间**计算（**50°/s**），纯视觉，**不进协议**（碰撞是圆形，与角度无关）。
  - 尺寸：缩放到**覆盖碰撞圆**（直径 = `2 × BH_R = 24` 逻辑单位）。
  - **圆形裁剪**（选项 B）：blit 时用圆形遮罩把方形图片裁成内切圆。
  - 缺图兜底：回退绘制实心圆；不得崩溃。
- **PC 呈现**：纯白背景（延续彩蛋主题）；顶部中央新增 **BOSS 血条**（剩余 HP/15）；双方生命 HUD 保留；BOSS 死亡 → 中央爆炸 + `BOSS DEFEATED!` 结算画面。
- **协议扩展**：状态帧新增传输 **BOSS 血量** 与 **BOSS 子弹**（复用子弹区机制，`FRAME_TX_LEN` 由宏推导，禁止写死偏移）。字节布局见 `docs/protocol.md`，最终帧长见 7.5。

### 7.5 温度传感器技能 —— 船头护盾 (Temperature Shield)

> 已与用户对齐，实现时严格遵守。

- **触发**：温度传感器 `Rt`（10K/3950 NTC），`calc_temp()` 查表+线性插值换算 0.1°C（移植 hw8）。**温度 > 30.0°C**（`SHIELD_TEMP=300`）时护盾激活，**常驻无冷却**；手放上约 15s 稳定到 ~33°C，手拿开后温度回落自动关闭（慢热响应天然防抖）。
- **双方都有**：Host 读本板 `Rt`（P1）；Slave 读本板 `Rt` 本地判定后经 RS485 上行 1 字节护盾标志（帧 4→5 字节）。
- **护盾几何**：船头前方**填充 75° 扇形**（正前 ±37.5°），半径 `SHIELD_R=15`，随船头朝向旋转；PC 渲染为弧线（P1 青色、P2 橙色）。
- **碰撞规则**：敌方子弹进入「距船 < SHIELD_R 且 与船头夹角 < 半张角(37.5°)」即被挡消失、飞船不死（点积判定，8051 免三角函数，`SHIELD_COS2`=cos²(半张角)）。
  - PvP：P1 盾挡 P2 弹、P2 盾挡 P1 弹；合作战：双方盾挡 BOSS 弹。
  - 不挡黑洞、不挡己方子弹、不挡飞船/实体撞击；船死后护盾失效。
- **协议**：Host→PC 状态帧新增 1 字节 `shield`（bit0=P1、bit1=P2），位于 `easterEgg` 与 `bossHp` 之间 → `FRAME_TX_LEN = 20 + 6*BULLET_MAX + 3*BOSS_BULLET_MAX`；PC 解析后按飞船朝向绘制护盾弧线。
- **换算表**：`inc/temp.h`（Host 与 Slave 共用，Slave 端复制一份）。

## 8. 人机协同 Git 工作流规范 (Human-AI Git Workflow)

针对本项目包含底层硬件（STC15）与物理层联调的特性，开发全流程采取 **“AI 主导代码生成，人类主导硬件测试与版本控制”** 的协同模式。具体约束如下：

### 8.1 分支管理
- **分支状态识别**：在开始一项新任务前，Agent 需确认当前功能是否已有专属分支。
  - **若为全新功能（AI 新建提示）**：若为全新功能，Agent 需主动输出创建并切换新分支的命令，例如：
    ```bash
    git checkout -b feat/your-feature-name
    ```
  - **若为现有功能（AI 驻留提示）**：若功能开发仍在进行中，Agent 在输出代码前需显式提醒用户确认当前分支状态，例如：*“确认当前位于 `feat/your-feature-name` 测试分支。”*

### 8.2 代码迭代与测试循环
1. **Agent 编码**：Agent 根据需求，生成或修改对应的源文件（如 `.c`, `.h`, `.py`）。
2. **User 物理测试**：User 将代码在 Keil 编译，或烧录至 STB_B 学习板进行硬件实机运行验证。
3. **免提交 Debug**：若编译报错或单片机跑飞，User 需直接反馈错误信息给 Agent 以修正代码。**未通过硬件验证前，禁止执行 Git 提交操作。**

### 8.3 提交与主干合并
- **用户控制提交**：仅当代码在物理硬件上按预期工作时，User 才执行 `git commit`。
- **Commit Message 建议**：Agent 需在实现功能后主动提供符合语义化规范（Conventional Commits）的提交建议，例如：
  ```bash
  git add .
  git commit -m "feat(rs485): 增加从机按键状态的主动上报逻辑与帧头校验"
  ```
- **多步构建与合并**：当该分支的所有子功能均在实机上测试完毕且状态稳定后，由 User 负责将其合并回主分支 (`main`)。