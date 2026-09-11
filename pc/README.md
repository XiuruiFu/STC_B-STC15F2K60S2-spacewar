# Spacewar! PC Renderer

PC 端渲染程序 (仅负责渲染与音效，物理引擎与状态机运行在 Host MCU)。

## 依赖管理 (uv)

```bash
uv sync                 # 安装依赖
```

## 运行

```bash
# 自动选择第一个可用串口
uv run python main.py

# 指定串口
uv run python main.py --port COM3

# 列出串口
uv run python main.py --list

# 无硬件模拟模式
uv run python main.py --sim
```

## 协议

见 `../docs/protocol.md`。串口 115200，可变长状态帧（`20 + 6*BULLET_MAX + 3*BOSS_BULLET_MAX` 字节，与 `config.py` 联动）。