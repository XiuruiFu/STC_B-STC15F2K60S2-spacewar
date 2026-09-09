"""Spacewar! PC 渲染器统一配置 (E0 Config Infrastructure).

集中存放渲染参数（窗口尺寸、比例、颜色主题等），便于实践中动态调整。
后续 E2 昼夜背景的两套配色也在此扩展。
"""

# ================= 窗口 =================
WINDOW_WIDTH = 800
WINDOW_HEIGHT = 800

# ================= 逻辑场域 =================
FIELD = 256  # 与 Host 逻辑场 0~255 对应，PC 按屏幕比例缩放

# ================= 子弹 =================
BULLET_MAX = 4  # 每船同时在场子弹上限 (与 Host config.h BULLET_MAX 保持一致)

# ================= 渲染几何 (逻辑单位, 实际像素 = 值 * scale) =================
SHIP_SIZE = 10.0
BULLET_RADIUS = 3.0
BLACKHOLE_RADIUS = 12.0
EXPLOSION_RADIUS = 8.0
EXPLOSION_INNER_RATIO = 0.6

# 黑洞逻辑中心坐标 (与 Host 保持一致)
BLACKHOLE_X = 128
BLACKHOLE_Y = 128

# ================= 菜单项名称 =================
MENU_ITEMS = ["Start Game", "Clear Wins", "Exit"]

# ================= 背景图片 (7.1) =================
BG_IMAGE_DIR = "assets"      # 相对 pc/ 目录
BG_IMAGE_DAY = "day.jpg"     # 白天背景图(缺失时回退纯色背景)
BG_IMAGE_NIGHT = "night.jpg" # 夜晚背景图(缺失时回退纯色背景)

# ================= BOSS (彩蛋合作战 7.4) =================
BOSS_BULLET_MAX = 12         # 与 Host config.h BOSS_BULLET_MAX 一致
BOSS_HP = 15                 # 与 Host config.h BOSS_HP 一致 (用于血条)
BOSS_ROT_SPEED = 50.0        # BOSS 图片旋转速度 (度/秒, 纯视觉)
BOSS_IMAGE = "boss.jpg"      # BOSS 贴图 (pc/assets/ 下)
BOSS_BULLET_RADIUS = 3.0     # BOSS 子弹绘制半径 (逻辑单位)

# ================= 温度护盾 (温度传感器技能) =================
SHIELD_R = 15.0              # 护盾半径 (逻辑单位, 与 Host SHIELD_R 一致)
SHIELD_HALF_ANGLE = 37.5    # 护盾半张角 (度, 正前 ±37.5° = 75° 扇形)
COLOR_SHIELD_P1 = (0, 200, 255)   # P1 护盾弧线颜色 (青色)
COLOR_SHIELD_P2 = (255, 120, 0)   # P2 护盾弧线颜色 (橙色)

# ================= 颜色主题 (RGB) =================
COLOR_BG_MENU = (10, 10, 30)
COLOR_BG_GAME = (0, 0, 0)
COLOR_BG_GAMEOVER = (20, 20, 20)

COLOR_TITLE = (255, 255, 255)
COLOR_MENU_SELECTED = (255, 255, 0)
COLOR_MENU_UNSELECTED = (180, 180, 180)
COLOR_MENU_WINS = (200, 200, 255)

COLOR_P1_SHIP = (0, 255, 120)
COLOR_P2_SHIP = (255, 80, 80)
COLOR_BULLET_P1 = (120, 255, 120)
COLOR_BULLET_P2 = (255, 160, 160)
COLOR_HUD_P1 = (0, 255, 120)
COLOR_HUD_P2 = (255, 80, 80)

# 白天背景配色 (E2 昼夜切换, bgMode=1 时使用)
COLOR_BG_GAME_DAY = (240, 245, 255)
COLOR_P1_SHIP_DAY = (0, 130, 60)
COLOR_P2_SHIP_DAY = (190, 40, 40)
COLOR_BULLET_P1_DAY = (0, 150, 90)
COLOR_BULLET_P2_DAY = (210, 80, 80)
COLOR_HUD_P1_DAY = (0, 110, 60)
COLOR_HUD_P2_DAY = (160, 30, 30)

# 彩蛋背景色 (7.3 彩蛋地图, 纯白)
COLOR_BG_GAME_EASTER = (255, 255, 255)

# BOSS 配色 (7.4 合作战)
COLOR_BOSS_BULLET = (220, 0, 120)     # BOSS 子弹颜色
COLOR_BOSS_FALLBACK = (90, 60, 150)   # BOSS 缺图回退实心圆颜色
COLOR_BOSS_HP_FILL = (220, 40, 40)    # BOSS 血条填充色
COLOR_BOSS_HP_BORDER = (60, 60, 60)   # BOSS 血条边框

COLOR_BLACKHOLE_FILL = (0, 0, 0)
COLOR_BLACKHOLE_RING = (120, 0, 120)

# 白洞配色 (7.2 引力系统, bgMode=1 白天时绘制)
COLOR_WHITEHOLE_FILL = (255, 255, 255)
COLOR_WHITEHOLE_RING = (255, 220, 120)

COLOR_EXPLOSION_OUTER = (255, 200, 0)
COLOR_EXPLOSION_INNER = (255, 120, 0)
