"""Spacewar! PC Renderer.

仅负责渲染: 通过串口接收 Host 发送的状态帧(长度由 BULLET_MAX 推导)并绘制画面、播放音效。
物理引擎与状态机均运行在 Host MCU 上。

串口帧格式见 docs/protocol.md。
"""
from __future__ import annotations

import argparse
import math
import os
import sys
import time

import serial

from config import (
    BG_IMAGE_DAY,
    BG_IMAGE_DIR,
    BG_IMAGE_NIGHT,
    BLACKHOLE_RADIUS,
    BLACKHOLE_X,
    BLACKHOLE_Y,
    BULLET_MAX,
    BULLET_RADIUS,
    COLOR_BG_GAMEOVER,
    COLOR_BG_GAME,
    COLOR_BG_GAME_DAY,
    COLOR_BG_GAME_EASTER,
    COLOR_BG_MENU,
    COLOR_BLACKHOLE_FILL,
    COLOR_BLACKHOLE_RING,
    COLOR_BULLET_P1,
    COLOR_BULLET_P1_DAY,
    COLOR_BULLET_P2,
    COLOR_BULLET_P2_DAY,
    COLOR_EXPLOSION_INNER,
    COLOR_EXPLOSION_OUTER,
    COLOR_HUD_P1,
    COLOR_HUD_P1_DAY,
    COLOR_HUD_P2,
    COLOR_HUD_P2_DAY,
    COLOR_MENU_SELECTED,
    COLOR_MENU_UNSELECTED,
    COLOR_MENU_WINS,
    COLOR_P1_SHIP,
    COLOR_P1_SHIP_DAY,
    COLOR_P2_SHIP,
    COLOR_P2_SHIP_DAY,
    COLOR_TITLE,
    COLOR_WHITEHOLE_FILL,
    COLOR_WHITEHOLE_RING,
    EXPLOSION_INNER_RATIO,
    EXPLOSION_RADIUS,
    FIELD,
    MENU_ITEMS,
    SHIP_SIZE,
    WINDOW_HEIGHT,
    WINDOW_WIDTH,
)

# ================= 协议常量 =================
HEAD_PC0 = 0xAA
HEAD_PC1 = 0x55
SER_READ_SIZE = 512
BULLETS_PER_PLAYER = BULLET_MAX  # 与 Host BULLET_MAX 一致

# 状态帧布局由 BULLET_MAX 推导: 帧头2+state/menuSel2+飞船8+子弹(2*BULLET_MAX*3)+胜场2+flags1+bgMode1+easterEgg1+校验和1
FRAME_P1WINS = 12 + 2 * BULLETS_PER_PLAYER * 3
FRAME_FLAGS = FRAME_P1WINS + 2
FRAME_BGMODE = FRAME_FLAGS + 1
FRAME_EASTER = FRAME_BGMODE + 1
FRAME_LEN = FRAME_EASTER + 2

ST_MENU = 0
ST_PLAYING = 1
ST_GAMEOVER = 2
ST_EXITED = 3

BG_NIGHT = 0
BG_DAY = 1

FLAG_P1_DEAD = 0x01
FLAG_P2_DEAD = 0x02
FLAG_P1_WIN = 0x04
FLAG_P2_WIN = 0x08

# 朝向角: 0=朝右, 逆时针增大, 1 字节 0~255 表示 0~360°
def angle_to_rad(a: int) -> float:
    return a * 2.0 * math.pi / 256.0


class Ship:
    def __init__(self) -> None:
        self.x = 0
        self.y = 0
        self.ang = 0
        self.lives = 0
        self.dead = False


class Bullet:
    def __init__(self) -> None:
        self.active = False
        self.x = 0
        self.y = 0


class GameState:
    def __init__(self) -> None:
        self.state = ST_MENU
        self.menu_sel = 0
        self.p1 = Ship()
        self.p2 = Ship()
        self.bullets = [Bullet() for _ in range(BULLETS_PER_PLAYER * 2)]
        self.p1_wins = 0
        self.p2_wins = 0
        self.flags = 0
        self.bg_mode = BG_NIGHT
        self.easter_egg = False
        self.frame_ok = False

    def parse(self, buf: bytes) -> bool:
        if len(buf) != FRAME_LEN:
            return False
        if buf[0] != HEAD_PC0 or buf[1] != HEAD_PC1:
            return False
        if sum(buf[0:FRAME_LEN - 1]) & 0xFF != buf[FRAME_LEN - 1]:
            return False
        self.state = buf[2]
        self.menu_sel = buf[3]
        self.p1.x = buf[4]
        self.p1.y = buf[5]
        self.p1.ang = buf[6]
        self.p1.lives = buf[7]
        self.p2.x = buf[8]
        self.p2.y = buf[9]
        self.p2.ang = buf[10]
        self.p2.lives = buf[11]
        n = 12
        for i in range(BULLETS_PER_PLAYER * 2):
            self.bullets[i].active = buf[n] != 0
            self.bullets[i].x = buf[n + 1]
            self.bullets[i].y = buf[n + 2]
            n += 3
        self.p1_wins = buf[FRAME_P1WINS]
        self.p2_wins = buf[FRAME_P1WINS + 1]
        self.flags = buf[FRAME_FLAGS]
        self.bg_mode = buf[FRAME_BGMODE]
        self.easter_egg = buf[FRAME_EASTER] != 0
        self.p1.dead = bool(self.flags & FLAG_P1_DEAD)
        self.p2.dead = bool(self.flags & FLAG_P2_DEAD)
        self.frame_ok = True
        return True


def map_coord(v: int, scale: float, offset: float) -> float:
    return v * scale + offset


class Renderer:
    def __init__(self, width: int = WINDOW_WIDTH, height: int = WINDOW_HEIGHT) -> None:
        import pygame

        self.pygame = pygame
        pygame.init()
        self.width = width
        self.height = height
        self.screen = pygame.display.set_mode((width, height))
        pygame.display.set_caption("Spacewar!")
        self.clock = pygame.time.Clock()
        self.scale = min(width, height) / FIELD
        self.ox = (width - FIELD * self.scale) / 2.0
        self.oy = (height - FIELD * self.scale) / 2.0
        self.font = pygame.font.Font(None, 28)
        self.big_font = pygame.font.Font(None, 48)
        self.snd_shoot = None
        self.snd_explode = None
        try:
            self.snd_shoot = pygame.mixer.Sound(buffer=_gen_tone(880, 40))
            self.snd_explode = pygame.mixer.Sound(buffer=_gen_tone(150, 120))
        except Exception:
            pass
        # 背景图片: 缩放铺满窗口; 缺失/加载失败时置 None, 绘制回退纯色背景
        self.bg_day = self._load_bg(BG_IMAGE_DAY, width, height)
        self.bg_night = self._load_bg(BG_IMAGE_NIGHT, width, height)

    def _load_bg(self, name: str, width: int, height: int):
        try:
            path = os.path.join(os.path.dirname(os.path.abspath(__file__)), BG_IMAGE_DIR, name)
            img = self.pygame.image.load(path)
            img = self.pygame.transform.smoothscale(img, (width, height))
            return img.convert()
        except Exception:
            return None

    def to_screen(self, x: int, y: int):
        return (map_coord(x, self.scale, self.ox), map_coord(y, self.scale, self.oy))

    def draw_ship(self, ship: Ship, color) -> None:
        if ship.lives == 0 and not ship.dead:
            return
        cx, cy = self.to_screen(ship.x, ship.y)
        ang = angle_to_rad(ship.ang)
        size = SHIP_SIZE * self.scale
        # 方向向量: (cos, -sin), 与 Host 物理引擎 diry=-sin 保持一致 (屏幕 Y 向下)
        dirx = math.cos(ang)
        diry = -math.sin(ang)
        nose = (cx + size * dirx, cy + size * diry)
        left_ang = ang + math.pi * 0.75
        right_ang = ang - math.pi * 0.75
        left = (cx + size * 0.6 * math.cos(left_ang), cy - size * 0.6 * math.sin(left_ang))
        right = (cx + size * 0.6 * math.cos(right_ang), cy - size * 0.6 * math.sin(right_ang))
        self.pygame.draw.polygon(self.screen, color, [nose, left, right])

    def draw_explosion(self, x: int, y: int) -> None:
        cx, cy = self.to_screen(x, y)
        r = EXPLOSION_RADIUS * self.scale
        self.pygame.draw.circle(self.screen, COLOR_EXPLOSION_OUTER, (int(cx), int(cy)), int(r))
        self.pygame.draw.circle(self.screen, COLOR_EXPLOSION_INNER, (int(cx), int(cy)), int(r * EXPLOSION_INNER_RATIO))

    def draw_bullet(self, b: Bullet, color) -> None:
        if not b.active:
            return
        cx, cy = self.to_screen(b.x, b.y)
        self.pygame.draw.circle(self.screen, color, (int(cx), int(cy)), int(BULLET_RADIUS * self.scale))

    def draw_blackhole(self, is_white: bool) -> None:
        cx, cy = self.to_screen(BLACKHOLE_X, BLACKHOLE_Y)
        r = BLACKHOLE_RADIUS * self.scale
        if is_white:
            fill, ring = COLOR_WHITEHOLE_FILL, COLOR_WHITEHOLE_RING
        else:
            fill, ring = COLOR_BLACKHOLE_FILL, COLOR_BLACKHOLE_RING
        self.pygame.draw.circle(self.screen, fill, (int(cx), int(cy)), int(r))
        self.pygame.draw.circle(self.screen, ring, (int(cx), int(cy)), int(r), 2)

    def draw_menu(self, gs: GameState) -> None:
        self.screen.fill(COLOR_BG_MENU)
        title = self.big_font.render("SPACEWAR!", True, COLOR_TITLE)
        self.screen.blit(title, (self.width // 2 - title.get_width() // 2, 80))
        for i, item in enumerate(MENU_ITEMS):
            color = COLOR_MENU_SELECTED if i == gs.menu_sel else COLOR_MENU_UNSELECTED
            txt = self.font.render(("> " if i == gs.menu_sel else "  ") + item, True, color)
            self.screen.blit(txt, (self.width // 2 - 120, 180 + i * 40))
        wins = self.font.render(
            f"P1 wins: {gs.p1_wins}    P2 wins: {gs.p2_wins}", True, COLOR_MENU_WINS
        )
        self.screen.blit(wins, (self.width // 2 - wins.get_width() // 2, 380))

    def draw_game(self, gs: GameState) -> None:
        if gs.easter_egg:
            bg = COLOR_BG_GAME_EASTER
            p1c = COLOR_P1_SHIP_DAY
            p2c = COLOR_P2_SHIP_DAY
            b1c = COLOR_BULLET_P1_DAY
            b2c = COLOR_BULLET_P2_DAY
            h1c = COLOR_HUD_P1_DAY
            h2c = COLOR_HUD_P2_DAY
            bg_img = None
        elif gs.bg_mode == BG_DAY:
            bg = COLOR_BG_GAME_DAY
            p1c = COLOR_P1_SHIP_DAY
            p2c = COLOR_P2_SHIP_DAY
            b1c = COLOR_BULLET_P1_DAY
            b2c = COLOR_BULLET_P2_DAY
            h1c = COLOR_HUD_P1_DAY
            h2c = COLOR_HUD_P2_DAY
            bg_img = self.bg_day
        else:
            bg = COLOR_BG_GAME
            p1c = COLOR_P1_SHIP
            p2c = COLOR_P2_SHIP
            b1c = COLOR_BULLET_P1
            b2c = COLOR_BULLET_P2
            h1c = COLOR_HUD_P1
            h2c = COLOR_HUD_P2
            bg_img = self.bg_night
        if bg_img is not None:
            self.screen.blit(bg_img, (0, 0))
        else:
            self.screen.fill(bg)
        self.draw_blackhole(gs.bg_mode == BG_DAY and not gs.easter_egg)
        if gs.p1.dead:
            self.draw_explosion(gs.p1.x, gs.p1.y)
        else:
            self.draw_ship(gs.p1, p1c)
        if gs.p2.dead:
            self.draw_explosion(gs.p2.x, gs.p2.y)
        else:
            self.draw_ship(gs.p2, p2c)
        for b in gs.bullets[:BULLETS_PER_PLAYER]:
            self.draw_bullet(b, b1c)
        for b in gs.bullets[BULLETS_PER_PLAYER:]:
            self.draw_bullet(b, b2c)
        # HUD
        hud1 = self.font.render(f"P1 lives: {gs.p1.lives}", True, h1c)
        hud2 = self.font.render(f"P2 lives: {gs.p2.lives}", True, h2c)
        self.screen.blit(hud1, (10, 10))
        self.screen.blit(hud2, (self.width - hud2.get_width() - 10, 10))

    def draw_gameover(self, gs: GameState) -> None:
        self.screen.fill(COLOR_BG_GAMEOVER)
        if gs.flags & FLAG_P1_WIN:
            txt = self.big_font.render("PLAYER 1 WINS!", True, COLOR_P1_SHIP)
        elif gs.flags & FLAG_P2_WIN:
            txt = self.big_font.render("PLAYER 2 WINS!", True, COLOR_P2_SHIP)
        else:
            txt = self.big_font.render("GAME OVER", True, COLOR_TITLE)
        self.screen.blit(txt, (self.width // 2 - txt.get_width() // 2, self.height // 2 - 40))
        wins = self.font.render(
            f"P1 wins: {gs.p1_wins}    P2 wins: {gs.p2_wins}", True, COLOR_MENU_WINS
        )
        self.screen.blit(wins, (self.width // 2 - wins.get_width() // 2, self.height // 2 + 20))

    def render(self, gs: GameState) -> None:
        if gs.state == ST_MENU:
            self.draw_menu(gs)
        elif gs.state == ST_PLAYING:
            self.draw_game(gs)
        elif gs.state == ST_GAMEOVER:
            self.draw_gameover(gs)
        self.pygame.display.flip()


def _gen_tone(freq: int, ms: int) -> bytes:
    import array

    rate = 22050
    n = int(rate * ms / 1000)
    out = array.array("h")
    for i in range(n):
        out.append(int(16000 * math.sin(2 * math.pi * freq * i / rate)))
    return out.tobytes()


class SerialSource:
    """串口数据源, 从字节流中同步帧。"""

    def __init__(self, port: str, baud: int) -> None:
        self.ser = serial.Serial(port, baud, timeout=0)
        self.buf = bytearray()

    def read_frame(self) -> bytes | None:
        data = self.ser.read(SER_READ_SIZE)
        if data:
            self.buf.extend(data)
        # 丢弃头部非帧头的错位字节, 定位到帧头
        while len(self.buf) >= 2 and not (
            self.buf[0] == HEAD_PC0 and self.buf[1] == HEAD_PC1
        ):
            self.buf.pop(0)
        # 若不足一帧, 等待更多数据
        if len(self.buf) < FRAME_LEN:
            return None
        # 校验和校验, 防止误帧
        if (sum(self.buf[0:FRAME_LEN - 1]) & 0xFF) != self.buf[FRAME_LEN - 1]:
            self.buf.pop(0)
            return None
        frame = bytes(self.buf[0:FRAME_LEN])
        del self.buf[0:FRAME_LEN]
        return frame

    def close(self) -> None:
        self.ser.close()


def list_ports():
    from serial.tools import list_ports as lp

    return [p.device for p in lp.comports()]


def main() -> int:
    ap = argparse.ArgumentParser(description="Spacewar! PC Renderer")
    ap.add_argument("--port", help="串口设备, 如 COM3 / /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--list", action="store_true", help="列出可用串口")
    ap.add_argument("--sim", action="store_true", help="模拟模式(无串口, 显示演示画面)")
    args = ap.parse_args()

    if args.list:
        for p in list_ports():
            print(p)
        return 0

    gs = GameState()
    renderer = Renderer(WINDOW_WIDTH, WINDOW_HEIGHT)

    src = None
    if not args.sim:
        if not args.port:
            ports = list_ports()
            if not ports:
                print("未找到串口, 请用 --port 指定或 --sim 模拟", file=sys.stderr)
                return 1
            args.port = ports[0]
            print(f"使用串口 {args.port}")
        try:
            src = SerialSource(args.port, args.baud)
        except Exception as e:
            print(f"打开串口失败: {e}", file=sys.stderr)
            return 1

    running = True
    while running:
        for event in renderer.pygame.event.get():
            if event.type == renderer.pygame.QUIT:
                running = False

        if src is not None:
            # 每次循环读尽缓冲中所有帧, 只取最新一帧渲染, 避免帧堆积导致显示延迟
            frame = src.read_frame()
            while frame is not None:
                gs.parse(frame)
                frame = src.read_frame()

        # Host 发送 ST_EXITED 时, 直接关闭窗口退出程序
        if gs.state == ST_EXITED:
            running = False
            continue

        renderer.render(gs)
        renderer.clock.tick(100)

    if src is not None:
        src.close()
    renderer.pygame.quit()
    return 0


if __name__ == "__main__":
    sys.exit(main())
