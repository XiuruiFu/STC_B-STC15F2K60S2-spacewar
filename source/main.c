#include "STC15F2K60S2.H"
#include "sys.H"
#include "displayer.H"
#include "key.H"
#include "adc.H"
#include "beep.H"
#include "uart1.H"
#include "uart2.H"
#include "DS1302.H"
#include "sin_table.h"

code unsigned long SysClock = 11059200;   // 11.0592MHz

#ifdef _displayer_H_
code char decode_table[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x00,0x08,0x40,0x01, 0x41, 0x48,
                            0x3f|0x80,0x06|0x80,0x5b|0x80,0x4f|0x80,0x66|0x80,0x6d|0x80,0x7d|0x80,0x07|0x80,0x7f|0x80,0x6f|0x80};
#endif

/* ================= 通信帧头 ================= */
#define HEAD_PC0    0xAA
#define HEAD_PC1    0x55
#define HEAD_SLV0   0xA5
#define HEAD_SLV1   0x5A

/* ================= 状态机 ================= */
#define ST_MENU     0
#define ST_PLAYING  1
#define ST_GAMEOVER 2
#define ST_EXITED   3

/* ================= 按键掩码位 ================= */
#define K_FWD   0x01
#define K_BACK  0x02
#define K_FIRE  0x04
#define K_LEFT  0x08
#define K_RIGHT 0x10
#define K_MENUUP    0x20   /* 菜单上移 (NavLeft) */
#define K_MENUDOWN  0x40   /* 菜单下移 (NavRight) */

/* ================= 物理常量 ================= */
#define FIELD        256
#define BH_X         128
#define BH_Y         128
#define BH_R         12
#define SHIP_R       6
#define BULLET_R     2
#define THRUST       0.05f
#define ROT_SPEED    2
#define MAX_SPEED    2.0f
#define BULLET_SPEED 2.0f
#define BULLET_LIFE  150
#define RESPAWN_TICKS 100
#define LIVES_MAX    3

/* ================= 飞船结构 ================= */
typedef struct {
    float x, y;
    float vx, vy;
    unsigned char ang;
    unsigned char lives;
    unsigned char respawn;   /* 重生倒计时, 0=在场 */
    unsigned char active;    /* 1=在场可见 */
} Ship;

/* ================= 子弹结构 ================= */
typedef struct {
    unsigned char active;
    float x, y;
    float vx, vy;
    unsigned int life;
} Bullet;

/* ================= 全局状态 ================= */
xdata Ship ship1, ship2;
xdata Bullet bullet1, bullet2;

xdata unsigned char g_state;       /* 状态机 */
xdata unsigned char g_menuSel;     /* 菜单选中项 */
xdata unsigned char p1_keys;       /* P1 按住状态掩码 */
xdata unsigned char p2_keys;       /* P2 按住状态掩码 */
xdata unsigned char p2_keys_prev;  /* P2 上一帧掩码(用于开火边沿) */
xdata unsigned char p1_fire_edge;  /* P1 开火边沿标志 */
xdata unsigned char p1wins, p2wins;
xdata unsigned char g_flags;       /* bit0/1 爆炸特效, bit2/3 胜者 */
xdata unsigned char gameover_tick; /* GAMEOVER 停留计时 */
xdata unsigned int  rs485_timeout; /* RS485 接收超时计数 */

xdata unsigned char uart2_rx[4];   /* Slave 按键帧接收缓冲 */
xdata unsigned char uart1_tx[24];  /* Host->PC 发送缓冲(须全局, 异步发送期间不覆盖) */
code  unsigned char slv_head[2] = {HEAD_SLV0, HEAD_SLV1};

/* ================= 随机数 (简单 LCG) ================= */
xdata unsigned int rnd_seed;
unsigned int rnd(void) {
    rnd_seed = rnd_seed * 1103515245u + 12345u;
    return (rnd_seed >> 8) & 0xFFFF;
}

/* ================= 菜单导航 ================= */
void menu_move(signed char dir) {
    signed char s = (signed char)g_menuSel + dir;
    if (s < 0) s = 2;
    if (s > 2) s = 0;
    g_menuSel = (unsigned char)s;
}

void menu_confirm(void) {
    switch (g_menuSel) {
        case 0: /* 进入游戏 -> 重置并开战 */
            ship1.x = 64;  ship1.y = 128; ship1.vx = 0; ship1.vy = 0;
            ship1.ang = 0; ship1.lives = LIVES_MAX; ship1.respawn = 0; ship1.active = 1;
            ship2.x = 192; ship2.y = 128; ship2.vx = 0; ship2.vy = 0;
            ship2.ang = 128; ship2.lives = LIVES_MAX; ship2.respawn = 0; ship2.active = 1;
            bullet1.active = 0; bullet2.active = 0;
            g_flags = 0;
            g_state = ST_PLAYING;
            SetBeep(800, 8);
            break;
        case 1: /* 总胜场清零 */
            p1wins = 0; p2wins = 0;
            NVM_Write(0, 0);
            NVM_Write(1, 0);
            NVM_Write(2, 0x5A);
            SetBeep(1000, 8);
            break;
        case 2: /* 退出游戏 */
            g_state = ST_EXITED;
            SetBeep(400, 10);
            break;
    }
}

/* ================= 重生 ================= */
void respawn_ship(Ship *s) {
    s->x = (float)(rnd() & 0xFF);
    s->y = (float)(rnd() & 0xFF);
    s->vx = 0.0f; s->vy = 0.0f;
    s->ang = (unsigned char)(rnd() & 0xFF);
    s->respawn = 0;
    s->active = 1;
    /* 清除对应玩家的爆炸特效标志 */
    if (s == &ship1) g_flags &= ~0x01;
    if (s == &ship2) g_flags &= ~0x02;
}

/* ================= 发射子弹 ================= */
void fire_bullet(Ship *s, Bullet *b) {
    float dirx, diry;
    unsigned char idx;
    idx = (unsigned char)(s->ang + 64);
    dirx = sin_table[idx];
    diry = -sin_table[s->ang];
    b->x = s->x + dirx * (float)(SHIP_R + 2);
    b->y = s->y + diry * (float)(SHIP_R + 2);
    b->vx = s->vx + dirx * BULLET_SPEED;
    b->vy = s->vy + diry * BULLET_SPEED;
    b->life = BULLET_LIFE;
    b->active = 1;
}

/* ================= 环绕边界 ================= */
float wrap_coord(float v) {
    if (v >= (float)FIELD) v -= (float)FIELD;
    if (v < 0.0f) v += (float)FIELD;
    return v;
}

/* ================= 距离平方(考虑环绕最近距离) ================= */
float dist2_wrap(float x1, float y1, float x2, float y2) {
    float dx, dy;
    dx = x1 - x2; if (dx < 0) dx = -dx;
    dy = y1 - y2; if (dy < 0) dy = -dy;
    if (dx > (float)(FIELD/2)) dx = (float)FIELD - dx;
    if (dy > (float)(FIELD/2)) dy = (float)FIELD - dy;
    return dx*dx + dy*dy;
}

/* ================= 飞船死亡 ================= */
void kill_ship(unsigned char player) {
    Ship *s;
    Bullet *b;
    if (player == 0) { s = &ship1; b = &bullet1; }
    else             { s = &ship2; b = &bullet2; }
    if (s->active == 0) return;
    s->lives--;
    b->active = 0;
    s->active = 0;
    s->respawn = RESPAWN_TICKS;
    g_flags |= (player == 0) ? 0x01 : 0x02;   /* 爆炸特效 */
    SetBeep(200, 15);
}

/* ================= 更新飞船 ================= */
void update_ship(Ship *s, unsigned char keys) {
    float dirx, diry;
    unsigned char idx;
    if (s->active == 0) {
        if (s->respawn > 0) {
            s->respawn--;
            if (s->respawn == 0 && s->lives > 0) respawn_ship(s);
        }
        return;
    }
    /* 旋转 */
    if (keys & K_LEFT)  s->ang += ROT_SPEED;
    if (keys & K_RIGHT) s->ang -= ROT_SPEED;
    /* 推力 */
    if (keys & (K_FWD | K_BACK)) {
        idx = (unsigned char)(s->ang + 64);
        dirx = sin_table[idx];
        diry = -sin_table[s->ang];
        if (keys & K_FWD)  { s->vx += dirx * THRUST; s->vy += diry * THRUST; }
        if (keys & K_BACK) { s->vx -= dirx * THRUST; s->vy -= diry * THRUST; }
    }
    /* 限速 */
    if (s->vx > MAX_SPEED) s->vx = MAX_SPEED;
    if (s->vx < -MAX_SPEED) s->vx = -MAX_SPEED;
    if (s->vy > MAX_SPEED) s->vy = MAX_SPEED;
    if (s->vy < -MAX_SPEED) s->vy = -MAX_SPEED;
    /* 积分 */
    s->x += s->vx;
    s->y += s->vy;
    s->x = wrap_coord(s->x);
    s->y = wrap_coord(s->y);
}

/* ================= 更新子弹 ================= */
void update_bullet(Bullet *b) {
    if (b->active == 0) return;
    b->x += b->vx;
    b->y += b->vy;
    b->x = wrap_coord(b->x);
    b->y = wrap_coord(b->y);
    if (b->life > 0) b->life--;
    else b->active = 0;
}

/* ================= 碰撞检测 ================= */
void check_collisions(void) {
    float r2;
    /* 黑洞吞飞船 */
    if (ship1.active && dist2_wrap(ship1.x, ship1.y, (float)BH_X, (float)BH_Y) < (float)(BH_R+SHIP_R)*(BH_R+SHIP_R))
        kill_ship(0);
    if (ship2.active && dist2_wrap(ship2.x, ship2.y, (float)BH_X, (float)BH_Y) < (float)(BH_R+SHIP_R)*(BH_R+SHIP_R))
        kill_ship(1);
    /* 黑洞吞子弹 */
    if (bullet1.active && dist2_wrap(bullet1.x, bullet1.y, (float)BH_X, (float)BH_Y) < (float)(BH_R+BULLET_R)*(BH_R+BULLET_R))
        bullet1.active = 0;
    if (bullet2.active && dist2_wrap(bullet2.x, bullet2.y, (float)BH_X, (float)BH_Y) < (float)(BH_R+BULLET_R)*(BH_R+BULLET_R))
        bullet2.active = 0;
    /* 子弹命中飞船 */
    r2 = (float)(SHIP_R+BULLET_R)*(SHIP_R+BULLET_R);
    if (bullet1.active && ship2.active && dist2_wrap(bullet1.x, bullet1.y, ship2.x, ship2.y) < r2) {
        bullet1.active = 0;
        kill_ship(1);
    }
    if (bullet2.active && ship1.active && dist2_wrap(bullet2.x, bullet2.y, ship1.x, ship1.y) < r2) {
        bullet2.active = 0;
        kill_ship(0);
    }
}

/* ================= 胜负判定 ================= */
void check_winner(void) {
    if (g_state != ST_PLAYING) return;
    if (ship1.lives == 0 && ship1.respawn == 0 && ship1.active == 0) {
        /* P1 判负, P2 胜 */
        p2wins++;
        NVM_Write(1, p2wins);
        g_flags |= 0x08;   /* P2 胜 */
        g_state = ST_GAMEOVER;
        gameover_tick = 200;  /* 2 秒后回菜单 */
        SetBeep(500, 30);
    }
    else if (ship2.lives == 0 && ship2.respawn == 0 && ship2.active == 0) {
        p1wins++;
        NVM_Write(0, p1wins);
        g_flags |= 0x04;   /* P1 胜 */
        g_state = ST_GAMEOVER;
        gameover_tick = 200;
        SetBeep(500, 30);
    }
}

/* ================= 组帧发送 Host->PC ================= */
void send_frame(void) {
    unsigned char sum, i;
    uart1_tx[0] = HEAD_PC0;
    uart1_tx[1] = HEAD_PC1;
    uart1_tx[2] = g_state;
    uart1_tx[3] = g_menuSel;
    uart1_tx[4] = (unsigned char)ship1.x;
    uart1_tx[5] = (unsigned char)ship1.y;
    uart1_tx[6] = ship1.ang;
    uart1_tx[7] = ship1.lives;
    uart1_tx[8] = (unsigned char)ship2.x;
    uart1_tx[9] = (unsigned char)ship2.y;
    uart1_tx[10] = ship2.ang;
    uart1_tx[11] = ship2.lives;
    uart1_tx[12] = bullet1.active;
    uart1_tx[13] = (unsigned char)bullet1.x;
    uart1_tx[14] = (unsigned char)bullet1.y;
    uart1_tx[15] = 0;
    uart1_tx[16] = bullet2.active;
    uart1_tx[17] = (unsigned char)bullet2.x;
    uart1_tx[18] = (unsigned char)bullet2.y;
    uart1_tx[19] = 0;
    uart1_tx[20] = p1wins;
    uart1_tx[21] = p2wins;
    uart1_tx[22] = g_flags;
    sum = 0;
    for (i = 0; i < 23; i++) sum += uart1_tx[i];
    uart1_tx[23] = sum;
    Uart1Print(uart1_tx, 24);
}

/* ================= 数码管显示胜场 ================= */
void display_wins(void) {
    unsigned char d[8];
    unsigned char i;
    for (i = 0; i < 8; i++) d[i] = 10;  /* 全灭 */
    /* P1 胜场占 3 位 */
    d[0] = p1wins / 100;
    d[1] = (p1wins / 10) % 10;
    d[2] = p1wins % 10;
    /* 中间分隔 */
    d[3] = 12;  /* '-' */
    /* P2 胜场占 3 位 */
    d[4] = p2wins / 100;
    d[5] = (p2wins / 10) % 10;
    d[6] = p2wins % 10;
    d[7] = 10;
    Seg7Print(d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
}

/* ================= 回调 ================= */
void cb_10ms(void) {
    /* RS485 超时检测 */
    if (rs485_timeout > 0) rs485_timeout--;

    /* 菜单态: P2 菜单导航 + 确认 (边沿触发) */
    if (g_state == ST_MENU) {
        if ((p2_keys & K_MENUUP) && !(p2_keys_prev & K_MENUUP)) menu_move(-1);
        if ((p2_keys & K_MENUDOWN) && !(p2_keys_prev & K_MENUDOWN)) menu_move(+1);
        if ((p2_keys & K_FIRE) && !(p2_keys_prev & K_FIRE)) menu_confirm();
    }
    /* EXITED 态: P2 按确认键返回菜单 */
    if (g_state == ST_EXITED) {
        if ((p2_keys & K_FIRE) && !(p2_keys_prev & K_FIRE)) g_state = ST_MENU;
    }

    if (g_state == ST_PLAYING) {
        /* P1 开火边沿 */
        if (p1_fire_edge) {
            p1_fire_edge = 0;
            if (bullet1.active == 0 && ship1.active) fire_bullet(&ship1, &bullet1);
        }
        /* P2 开火边沿 */
        if ((p2_keys & K_FIRE) && !(p2_keys_prev & K_FIRE)) {
            if (bullet2.active == 0 && ship2.active) fire_bullet(&ship2, &bullet2);
        }

        update_ship(&ship1, p1_keys);
        update_ship(&ship2, p2_keys);
        update_bullet(&bullet1);
        update_bullet(&bullet2);
        check_collisions();
        check_winner();
    }
    else if (g_state == ST_GAMEOVER) {
        if (gameover_tick > 0) {
            gameover_tick--;
            if (gameover_tick == 0) {
                g_state = ST_MENU;
                g_flags = 0;
            }
        }
    }

    p2_keys_prev = p2_keys;

    send_frame();
    display_wins();
}

void cb_key(void) {
    unsigned char k;
    k = GetKeyAct(enumKey1);
    if (k == enumKeyPress) p1_keys |= K_BACK;
    else if (k == enumKeyRelease) p1_keys &= ~K_BACK;

    k = GetKeyAct(enumKey2);
    if (k == enumKeyPress) {
        if (g_state == ST_PLAYING) p1_fire_edge = 1;
        else if (g_state == ST_MENU) menu_confirm();
        else if (g_state == ST_EXITED) g_state = ST_MENU;
        SetBeep(900, 4);
    }
}

void cb_nav(void) {
    unsigned char k;
    /* Key3(前进) 与导航键 K3 共用 P1.7, 只能通过 ADC 读 */
    k = GetAdcNavAct(enumAdcNavKey3);
    if (k == enumKeyPress) p1_keys |= K_FWD;
    else if (k == enumKeyRelease) p1_keys &= ~K_FWD;

    /* 游戏态: 旋转 */
    k = GetAdcNavAct(enumAdcNavKeyDown);
    if (k == enumKeyPress) p1_keys |= K_LEFT;
    else if (k == enumKeyRelease) p1_keys &= ~K_LEFT;

    k = GetAdcNavAct(enumAdcNavKeyUp);
    if (k == enumKeyPress) p1_keys |= K_RIGHT;
    else if (k == enumKeyRelease) p1_keys &= ~K_RIGHT;

    /* 菜单态: 上下选择 */
    if (g_state == ST_MENU) {
        k = GetAdcNavAct(enumAdcNavKeyLeft);
        if (k == enumKeyPress) menu_move(-1);
        k = GetAdcNavAct(enumAdcNavKeyRight);
        if (k == enumKeyPress) menu_move(+1);
    }
}

void cb_uart2(void) {
    unsigned char sum;
    sum = uart2_rx[0] + uart2_rx[1] + uart2_rx[2];
    if (uart2_rx[3] == sum) {
        p2_keys = uart2_rx[2];
        rs485_timeout = 50;  /* 500ms 内视为在线 */
    }
}

void cb_led(void) {
    /* LED: 最高位指示 RS485 在线, 低4位指示当前菜单项(菜单态) */
    unsigned char led = 0;
    if (rs485_timeout > 0) led |= 0x80;
    if (g_state == ST_MENU) led |= (1 << g_menuSel);
    LedPrint(led);
}

/* ================= 初始化 NVM ================= */
void init_nvm(void) {
    struct_DS1302_RTC t;
    t.second = 0; t.minute = 0; t.hour = 0;
    t.day = 1; t.month = 1; t.week = 1; t.year = 24;
    DS1302Init(t);
    if (NVM_Read(2) != 0x5A) {
        NVM_Write(0, 0);
        NVM_Write(1, 0);
        NVM_Write(2, 0x5A);
        p1wins = 0; p2wins = 0;
    } else {
        p1wins = NVM_Read(0);
        p2wins = NVM_Read(1);
    }
}

/* ================= 主程序 ================= */
void main(void) {
    DisplayerInit();
    KeyInit();
    AdcInit(ADCexpEXT);
    BeepInit();
    Uart1Init(115200);
    Uart2Init(38400, Uart2Usedfor485);
    SetUart2Rxd(uart2_rx, 4, slv_head, 2);

    init_nvm();

    g_state = ST_MENU;
    g_menuSel = 0;
    p1_keys = 0; p2_keys = 0; p2_keys_prev = 0;
    p1_fire_edge = 0;
    g_flags = 0;
    gameover_tick = 0;
    rs485_timeout = 0;
    rnd_seed = 0x1234;
    ship1.active = 0; ship2.active = 0;
    bullet1.active = 0; bullet2.active = 0;
    SetDisplayerArea(0, 7);
    Seg7Print(10, 10, 10, 10, 10, 10, 10, 10);
    LedPrint(0);

    SetEventCallBack(enumEventSys10mS, cb_10ms);
    SetEventCallBack(enumEventKey, cb_key);
    SetEventCallBack(enumEventNav, cb_nav);
    SetEventCallBack(enumEventUart2Rxd, cb_uart2);
    SetEventCallBack(enumEventSys100mS, cb_led);

    MySTC_Init();

    while (1) {
        MySTC_OS();
    }
}
