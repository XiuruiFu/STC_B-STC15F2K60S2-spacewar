#include "STC15F2K60S2.H"
#include "sys.H"
#include "displayer.H"
#include "key.H"
#include "adc.H"
#include "beep.H"
#include "music.h"
#include "temp.h"
#include "uart2.H"
#include "hall.H"
#include "IR.h"
#include "vib.h"
#include "FM_Radio.h"

code unsigned long SysClock = 11059200;   // 11.0592MHz

#ifdef _displayer_H_
code char decode_table[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x00,0x08,0x40,0x01, 0x41, 0x48,
                            0x3f|0x80,0x06|0x80,0x5b|0x80,0x4f|0x80,0x66|0x80,0x6d|0x80,0x7d|0x80,0x07|0x80,0x7f|0x80,0x6f|0x80};
#endif

/* ================= 循环背景音乐 (硬编码于从机) =================
 * 乐谱格式: 每两个字节一组 (音高, 时值)。音高 scale: 高4位音区(2=中央8度)、
 * 低3位唱名(1~7=do~si); 时值单位 1/16 拍。曲目: 小星星 (C大调, 24 小节)。
 */

// code unsigned char bgm_score[] = {
//     0x21,0x10, 0x21,0x10, 0x25,0x10, 0x25,0x10,  /* do do so so */
//     0x26,0x10, 0x26,0x10, 0x25,0x20,             /* la la so-- */
//     0x24,0x10, 0x24,0x10, 0x23,0x10, 0x23,0x10,  /* fa fa mi mi */
//     0x22,0x10, 0x22,0x10, 0x21,0x20,             /* re re do-- */
//     0x25,0x10, 0x25,0x10, 0x24,0x10, 0x24,0x10,  /* so so fa fa */
//     0x23,0x10, 0x23,0x10, 0x22,0x20,             /* mi mi re-- */
//     0x25,0x10, 0x25,0x10, 0x24,0x10, 0x24,0x10,  /* so so fa fa */
//     0x23,0x10, 0x23,0x10, 0x22,0x20,             /* mi mi re-- */
//     0x21,0x10, 0x21,0x10, 0x25,0x10, 0x25,0x10,  /* do do so so */
//     0x26,0x10, 0x26,0x10, 0x25,0x20,             /* la la so-- */
//     0x24,0x10, 0x24,0x10, 0x23,0x10, 0x23,0x10,  /* fa fa mi mi */
//     0x22,0x10, 0x22,0x10, 0x21,0x20              /* re re do-- */
// };

// code unsigned char bgm_score_contra[] = {
//     /* 第一句：噔~(2拍) 噔~(2拍) */
//     0x26,0x20, 0x33,0x20,             /* La(A4) Mi(E5) */
//     /* 滴滴滴滴 滴滴嘟~ (每音半拍，最后落脚1拍) */
//     0x36,0x08, 0x35,0x08, 0x33,0x08, 0x32,0x08, /* La(A5) So(G5) Mi(E5) Re(D5) */
//     0x31,0x08, 0x32,0x08, 0x33,0x10,  /* Do(C5) Re(D5) Mi(E5) */

//     /* 第二句：噔~(2拍) 噔~(2拍) */
//     0x26,0x20, 0x33,0x20,             /* La(A4) Mi(E5) */
//     /* 滴滴滴滴 滴滴嘟~（回归主音） */
//     0x36,0x08, 0x35,0x08, 0x33,0x08, 0x32,0x08, /* La(A5) So(G5) Mi(E5) Re(D5) */
//     0x31,0x08, 0x27,0x08, 0x26,0x10,  /* Do(C5) Si(B4) La(A4) */

//     /* 第三句：大三和弦向上爬音 (紧张感，每音半拍) */
//     0x24,0x08, 0x26,0x08, 0x31,0x08, 0x34,0x08, /* Fa(F4) La(A4) Do(C5) Fa(F5) */
//     0x33,0x08, 0x32,0x08, 0x31,0x08, 0x27,0x08, /* Mi(E5) Re(D5) Do(C5) Si(B4) */
//     0x31,0x40,                        /* Do(C5) -- 保持4拍 */

//     /* 第四句：继续爬音，情绪最高潮 */
//     0x25,0x08, 0x27,0x08, 0x32,0x08, 0x35,0x08, /* So(G4) Si(B4) Re(D5) So(G5) */
//     0x34,0x08, 0x33,0x08, 0x32,0x08, 0x31,0x08, /* Fa(F5) Mi(E5) Re(D5) Do(C5) */
//     0x32,0x40,                        /* Re(D5) -- 保持4拍 */

//     /* 尾句：快速下行回到原点 */
//     0x36,0x08, 0x35,0x08, 0x33,0x08, 0x32,0x08, /* La(A5) So(G5) Mi(E5) Re(D5) */
//     0x31,0x08, 0x27,0x08, 0x26,0x10,  /* Do(C5) Si(B4) La(A4) */
    
//     0x00,0x00
// };

code unsigned char bgm_score[] = {
    /* 第一句：噔~(1拍) 噔~(1拍) */
    0x26,0x10, 0x33,0x10,             /* La(A4) Mi(E5) */
    /* 滴滴滴滴 滴滴嘟~ (每音1/4拍，最后落脚半拍) */
    0x36,0x04, 0x35,0x04, 0x33,0x04, 0x32,0x04, /* La(A5) So(G5) Mi(E5) Re(D5) */
    0x31,0x04, 0x32,0x04, 0x33,0x08,  /* Do(C5) Re(D5) Mi(E5) */

    /* 第二句：噔~(1拍) 噔~(1拍) */
    0x26,0x10, 0x33,0x10,             /* La(A4) Mi(E5) */
    /* 滴滴滴滴 滴滴嘟~（回归主音） */
    0x36,0x04, 0x35,0x04, 0x33,0x04, 0x32,0x04, /* La(A5) So(G5) Mi(E5) Re(D5) */
    0x31,0x04, 0x27,0x04, 0x26,0x08,  /* Do(C5) Si(B4) La(A4) */

    /* 第三句：大三和弦向上爬音 (紧张感，每音1/4拍) */
    0x24,0x04, 0x26,0x04, 0x31,0x04, 0x34,0x04, /* Fa(F4) La(A4) Do(C5) Fa(F5) */
    0x33,0x04, 0x32,0x04, 0x31,0x04, 0x27,0x04, /* Mi(E5) Re(D5) Do(C5) Si(B4) */
    0x31,0x20,                        /* Do(C5) -- 保持2拍 */

    /* 第四句：继续爬音，情绪最高潮 */
    0x25,0x04, 0x27,0x04, 0x32,0x04, 0x35,0x04, /* So(G4) Si(B4) Re(D5) So(G5) */
    0x34,0x04, 0x33,0x04, 0x32,0x04, 0x31,0x04, /* Fa(F5) Mi(E5) Re(D5) Do(C5) */
    0x32,0x20,                        /* Re(D5) -- 保持2拍 */

    /* 尾句：快速下行回到原点 */
    0x36,0x04, 0x35,0x04, 0x33,0x04, 0x32,0x04, /* La(A5) So(G5) Mi(E5) Re(D5) */
    0x31,0x04, 0x27,0x04, 0x26,0x08,  /* Do(C5) Si(B4) La(A4) */
    
    0x00,0x00
};

/* 按键掩码位 (与 Host 协议一致) */
#define K_FWD   0x01
#define K_BACK  0x02
#define K_FIRE  0x04
#define K_LEFT  0x08
#define K_RIGHT 0x10
#define K_MENUUP    0x20   /* 菜单上移 (NavLeft) */
#define K_MENUDOWN  0x40   /* 菜单下移 (NavRight) */

#define HEAD_SLV0  0xA5
#define HEAD_SLV1  0x5A

#define EASTER_MAGIC 0xE1  /* 彩蛋魔数(红外发送给主机) */

/* 护盾激活温度阈值 (0.1°C, 与 Host config.h SHIELD_TEMP 保持一致) */
#define SHIELD_TEMP 300

xdata unsigned char keys;        /* 当前按住状态掩码 */
xdata unsigned char keys_prev;   /* 上一帧掩码 */
xdata unsigned char fire_edge;   /* 开火边沿 */
xdata unsigned char shield;      /* 护盾激活标志(温度>阈值=1) */
xdata unsigned int  send_tick;   /* 发送节拍 */
xdata unsigned char enable_music;
xdata unsigned char uart2_tx[5]; /* 发送缓冲(须全局, 异步发送期间不覆盖) */
code  unsigned char ir_tx[1] = {EASTER_MAGIC}; /* 彩蛋红外数据(不防抖, 霍尔触发即发) */
struct_FMRadio FM = {918, 6, 0xff, 0, 0xff};

void cb_key(void) {
    unsigned char k;
    k = GetKeyAct(enumKey1);
    if (k == enumKeyPress) keys |= K_BACK;
    else if (k == enumKeyRelease) keys &= ~K_BACK;

    k = GetKeyAct(enumKey2);
    if (k == enumKeyPress) {
        fire_edge = 1;
        /* 音效由主机统一发声, 从机不 Beep(避免与背景乐抢单音轨) */
    }
}

void cb_nav(void) {
    unsigned char k;
    /* Key3(前进) 与导航键 K3 共用 P1.7, 只能通过 ADC 读 */
    k = GetAdcNavAct(enumAdcNavKey3);
    if (k == enumKeyPress) keys |= K_FWD;
    else if (k == enumKeyRelease) keys &= ~K_FWD;

    k = GetAdcNavAct(enumAdcNavKeyDown);
    if (k == enumKeyPress) keys |= K_LEFT;
    else if (k == enumKeyRelease) keys &= ~K_LEFT;

    k = GetAdcNavAct(enumAdcNavKeyUp);
    if (k == enumKeyPress) keys |= K_RIGHT;
    else if (k == enumKeyRelease) keys &= ~K_RIGHT;

    /* 菜单导航键 (由 Host 按状态解释) */
    k = GetAdcNavAct(enumAdcNavKeyLeft);
    if (k == enumKeyPress) keys |= K_MENUUP;
    else if (k == enumKeyRelease) keys &= ~K_MENUUP;

    k = GetAdcNavAct(enumAdcNavKeyRight);
    if (k == enumKeyPress) keys |= K_MENUDOWN;
    else if (k == enumKeyRelease) keys &= ~K_MENUDOWN;
}

void send_keys(void) {
    unsigned char mask;
    unsigned char changed;
    mask = keys;
    if (fire_edge) {
        fire_edge = 0;
        mask |= K_FIRE;   /* 开火边沿并入本帧 */
    }
    changed = (mask != keys_prev);
    if (!changed && send_tick > 0) return;
    keys_prev = mask;
    uart2_tx[0] = HEAD_SLV0;
    uart2_tx[1] = HEAD_SLV1;
    uart2_tx[2] = mask;
    uart2_tx[3] = shield;
    uart2_tx[4] = (unsigned char)(HEAD_SLV0 + HEAD_SLV1 + mask + shield);
    Uart2Print(uart2_tx, 5);
    if (!changed) send_tick = 10;   /* 无变化时每 100ms 保底发送一次(维持在线指示) */
}

void cb_10ms(void) {
    if (send_tick > 0) send_tick--;
    /* 读本板温度, 高于阈值则护盾激活 */
    shield = (calc_temp(GetADC().Rt) > SHIELD_TEMP) ? 1 : 0;
    send_keys();
}

void cb_led(void) {
    /* LED: 低5位直接反映按键状态, 便于调试 */
    LedPrint(keys);

    if( GetVibAct() == enumVibQuake) {
        if(enable_music == 0)  enable_music = 1;
        else                   enable_music = 0;
    }
    Seg7Print(2, 10, 10, 10, 10, 10, 10, enable_music);
    /* 背景乐循环: 播完(Stop)后自动重新播放 */
    if (enable_music != 0 && GetPlayerMode() != enumModePlay) {
        // MusicPlayerInit();
		SetMusic(100, 0xFA, bgm_score, sizeof(bgm_score), enumMscNull);
        SetPlayerMode(enumModePlay);
    }

    if(enable_music == 0) {
        SetPlayerMode(enumModePause);
    }
}

void cb_hall(void) {
    unsigned char h;
    h = GetHallAct();
    /* 磁场靠近或离开任一即触发彩蛋红外发射(不防抖) */
    if (h == enumHallGetClose || h == enumHallGetAway) {
        IrPrint(ir_tx, 1);
    }
}

void main(void) {
    DisplayerInit();
    KeyInit();
    AdcInit(ADCexpEXT);
    BeepInit();
    MusicPlayerInit();
    Uart2Init(38400, Uart2Usedfor485);
    HallInit();
    IrInit(NEC_R05d);
    VibInit();
	FMRadioInit(FM);
	
    keys = 0; keys_prev = 0; fire_edge = 0;
    shield = 0;
    send_tick = 0;
    enable_music = 1;
    SetDisplayerArea(0, 7);
    Seg7Print(10, 10, 10, 10, 10, 10, 10, 10);
    LedPrint(0);
    Seg7Print(2, 10, 10, 10, 10, 10, 10, 10);  /* 显示 '2' 表示 Player 2 */

    /* 循环背景音乐: 设置乐谱并开始播放(播完由 cb_led 自动重播) */
    SetMusic(100, 0xFA, bgm_score, sizeof(bgm_score), enumMscNull);
    SetPlayerMode(enumModePlay);

    SetEventCallBack(enumEventSys10mS, cb_10ms);
    SetEventCallBack(enumEventKey, cb_key);
    SetEventCallBack(enumEventNav, cb_nav);
    SetEventCallBack(enumEventSys100mS, cb_led);
    SetEventCallBack(enumEventHall, cb_hall);

    MySTC_Init();

    while (1) {
        MySTC_OS();
    }
}
