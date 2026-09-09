#ifndef _CONFIG_H_
#define _CONFIG_H_

/* =====================================================================
 * Spacewar! Host 固件统一配置 (E0 Config Infrastructure)
 * ---------------------------------------------------------------------
 * 集中存放所有可调参数，供物理引擎/状态机/后续扩展功能读取。
 * 改动一处即可生效，无需翻找 main.c 中散落的常量。
 * ===================================================================== */

/* ================= 场域与几何 ================= */
#define FIELD           256     /* 逻辑场域边长 (0~255 环绕) */
#define BH_X            128     /* 黑洞中心 X */
#define BH_Y            128     /* 黑洞中心 Y */
#define BH_R            12      /* 黑洞吞噬半径 (飞船/子弹中心进入即吞) */
#define SHIP_R          6       /* 飞船碰撞半径 */
#define BULLET_R        2       /* 子弹碰撞半径 */

/* ================= 运动物理 ================= */
#define THRUST          0.05f   /* 双向推力加速度 (每 10ms tick) */
#define ROT_SPEED       2       /* 旋转角速度 (每 tick, 单位 1/256 圈) */
#define MAX_SPEED       2.0f    /* 速度上限 (每 tick 位移) */
#define GRAVITY         3.0f    /* 引力/斥力常数 (每 tick 加速度) */
#define GRAVITY_MIN_R   6       /* 距离下限截断 (r² 取下限防止发散, 实际 r<BH_R+SHIP_R 已死亡) */

/* ================= 子弹 ================= */
#define BULLET_SPEED    2.0f    /* 子弹速度 */
#define BULLET_LIFE     200     /* 子弹存活时长 (10ms ticks, 200 = 2.0s) */
#define BULLET_MAX      4       /* 每船同时在场子弹上限 (改此值会改变协议帧长, 须同步 pc/config.py 的 BULLET_MAX) */

/* ================= 生命与重生 ================= */
#define LIVES_MAX       3       /* 每局初始生命数 */
#define RESPAWN_TICKS   100     /* 死亡后重生延时 (10ms ticks, 100 = 1s) */

/* ================= BOSS (彩蛋合作战) ================= */
#define BOSS_HP            15      /* BOSS 血量 (每发玩家子弹命中扣 1) */
#define BOSS_BULLET_SPEED  1.2f    /* BOSS 子弹速度 */
#define BOSS_BULLET_R      3       /* BOSS 子弹碰撞半径 */
#define BOSS_BULLET_LIFE   120     /* BOSS 子弹寿命 (10ms ticks, 120 = 1.2s) */
#define BOSS_BULLET_MAX    12      /* BOSS 子弹同屏上限 (改此值会改变协议帧长, 须同步 pc/config.py) */
#define BOSS_FIRE_INTERVAL 300     /* BOSS 射击间隔 (10ms ticks, 300 = 3s) */
#define BOSS_FIRE_COUNT    8       /* 每轮散射发数 */
#define BOSS_FIRE_ROT      11      /* 每轮散射整体旋转步进 (256 制角度, ~15.5°) */

/* ================= 光敏阈值 (E2 昼夜背景) ================= */
#define LIGHT_THRESHOLD 30      /* 开始游戏瞬间 Rop > 阈值 -> 白天, 否则夜晚 (实测标定) */

/* ================= 护盾技能 (温度传感器) ================= */
#define SHIELD_R        15      /* 护盾半径 (船头前方, 逻辑单位) */
#define SHIELD_TEMP     300     /* 护盾激活温度阈值 (0.1°C, 300 = 30.0°C) */
#define SHIELD_COS2     0.6294f /* cos²(护盾半张角): 总张角 75° -> 半张角 37.5°, cos²≈0.6294 */

#endif /* _CONFIG_H_ */
