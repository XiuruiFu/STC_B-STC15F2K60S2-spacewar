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

## 4. 开发与调试经验 (Hard-Won Lessons)

> 以下为本项目联调过程中总结的**关键陷阱**，实现/修改固件时必须遵守。违反这些会导致难以排查的隐性 bug。

1. **Keil 授权**：Keil µVision 必须使用**正版/已授权**版本。评估版 (Eval Version) 链接器限制 code ≤ 2KB，无法编译本工程（浮点库即超限）。报错特征：`FATAL ERROR L250: CODE SIZE LIMIT IN RESTRICTED VERSION EXCEEDED`。

2. **Key3 (前进) 不能用 `GetKeyAct(enumKey3)`**：Key3 与导航摇杆的 K3 共用 P1.7 端口。BSP 规定 P1.7 上的 Key3 **必须**用 `GetAdcNavAct(enumAdcNavKey3)` 读取（触发 `enumEventNav` 事件）。`GetKeyAct(enumKey3)` 永远读不到，前进键会完全失效。Host 与 Slave 两端都要遵守。

3. **导航键/摇杆必须 `AdcInit(ADCexpEXT)`**：凡需用 `GetAdcNavAct()` 检测导航键（上/下/左/右/K3），`AdcInit()` 参数必须是 `ADCexpEXT`。用 `ADCincEXT` 会导致导航键事件完全不触发（症状：拨动摇杆无反应）。`ADCexpEXT` 会占用 EXT 接口的 P1.0/P1.1 作 ADC。

4. **UART 异步发送缓冲区必须用全局 `xdata` 数组**：`Uart1Print`/`Uart2Print` 是异步的（调用返回约 1µs，后台继续发）。若传入**函数内局部栈数组**，函数返回后该内存被其他函数的局部变量覆盖（C51 对 data 区做 Overlay 复用），导致帧头/数据被破坏——典型症状：帧头 `0xAA 0x55` 收到 `0xAA 0x00`（0x55 被覆盖）。**所有发送缓冲必须声明为全局 `xdata` 数组**（Host 的 `uart1_tx[24]`、Slave 的 `uart2_tx[4]` 已如此）。

5. **`enumEventNav` 回调里要轮询所有关心的导航键**：`GetAdcNavAct()` 每次只返回一个键的事件（查询一次后该键事件清零）。在 `cb_nav` 里应逐个调用，不要只查一个。

6. **P2 按键边沿检测**：Host 在 `cb_10ms` 里用 `(p2_keys & BIT) && !(p2_keys_prev & BIT)` 检测 P2 的边沿（开火/菜单确认）。**同一 tick 内不能对同一边沿做两次消费**——若先 `menu_confirm()` 切到新状态，又在同一函数体后续分支再次检测同一边沿，会立刻被改回。`p2_keys_prev = p2_keys` 更新须放在所有边沿检测之后。

7. **PC 串口帧同步**：PC 端 `read_frame` 用 `timeout=0` 非阻塞读取，`while` 循环读尽缓冲、只取最新一帧渲染。若每 tick 只消费一帧或阻塞读，帧堆积会导致显示延迟数秒。

8. **显示器初始化**：`DisplayerInit()` 后须手动 `Seg7Print(10,10,...)`（全灭）+ `LedPrint(0)`（灭灯）做初始清屏。

---

## 5. 需求规格 (Requirements Specification)

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
- **协议帧**：Slave→Host 4 字节；Host→PC 24 字节。详见 `docs/protocol.md`（三端唯一权威契约）。
- **可调物理常量**（当前散落在 `source/main.c`，E0 将迁入 `config.h`）：`THRUST=0.05f`（推力）、`ROT_SPEED=2`（转速）、`MAX_SPEED=2.0f`（限速）、`BULLET_SPEED=2.0f`（子弹速度）、`BULLET_LIFE=150`（子弹寿命 ticks）、`LIVES_MAX=3`（生命）、`BH_R=12`（黑洞半径）、`FIELD=256`（场域）、`RESPAWN_TICKS=100`（重生延时）。

---

## 7. 扩展功能规划 (Optional Extensions)

> 下述扩展按依赖顺序排列。**E0 是基础设施，须最先做**，其余功能依赖它读取配置。

### E0 统一配置管理 (Config Infrastructure) —— 基础设施，最先实现
- 引入统一的配置管理文件，集中存放所有可调参数（物理常量、阈值、游戏数值等），供后续功能读取。
- C51 侧：新建 `config.h`，用 `#define` 集中定义 Host 固件的所有可调常量（推力、转速、限速、子弹速度/寿命/数量、重力、边界、生命数、黑洞半径、光敏阈值等），`main.c` 不再散落 `#define`。
- PC 侧：新增 `pc/config.py`（或 `config.json`），集中存放渲染参数（背景色、比例、颜色主题等）。
- **目标**：用户（尤其光敏阈值）可在实践中动态调整，改一处即可，无需翻找代码。

### E1 多弹系统 (3 发子弹 + 存活时间缩短)
- 每船同时在场子弹上限由 **1 发 → 3 发**（子弹结构改为数组 `Bullet[3]`）。
- 子弹存活时间由原 `150`（10ms 节拍 ×150 = 1.5 秒）缩短至 **原 75%**（≈ `112` ticks，即 1.125 秒）。
- 协议扩展：Host→PC 状态帧需携带最多 6 发子弹的坐标/朝向/在场标志，帧长相应扩大（须同步更新 `docs/protocol.md` 与 PC 解析器）。
- 开火逻辑：按住/边沿触发时，若已有 3 发在场则不再发射（或替换最老的那发）。

### E2 昼夜背景切换 (光敏电阻阈值判定)
- 引入白天/夜晚两种画面背景；Host 采集光敏电阻 `Rop`（`GetADC().Rop`，GL5516，10bit，约 9ms 一次轮询，无需事件）与阈值比较，决定当前背景模式。
- 通过状态帧新增 1 字节 `bgMode`（或复用 `flags` 未用位）下发给 PC，PC 据此渲染白天/夜晚两套配色（背景色、飞船/子弹/黑洞配色等）。
- **光敏阈值**（及可能的滞回区间）作为可调参数放入 E0 的 config，用户在实践中动态标定。
- 触发方式（阈值正比/反比、白天=亮=背景白 等具体映射）实施前与用户确认。

### E3 其它玩法
- 如黑洞引力、道具/补给、连发冷却、护盾等，可后续再议。

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