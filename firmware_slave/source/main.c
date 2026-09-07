#include "STC15F2K60S2.H"
#include "sys.H"
#include "displayer.H"
#include "key.H"
#include "adc.H"
#include "uart2.H"

code unsigned long SysClock = 11059200;   // 11.0592MHz

#ifdef _displayer_H_
code char decode_table[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x00,0x08,0x40,0x01, 0x41, 0x48,
                            0x3f|0x80,0x06|0x80,0x5b|0x80,0x4f|0x80,0x66|0x80,0x6d|0x80,0x7d|0x80,0x07|0x80,0x7f|0x80,0x6f|0x80};
#endif

/* 按键掩码位 (与 Host 协议一致) */
#define K_FWD   0x01
#define K_BACK  0x02
#define K_FIRE  0x04
#define K_LEFT  0x08
#define K_RIGHT 0x10

#define HEAD_SLV0  0xA5
#define HEAD_SLV1  0x5A

xdata unsigned char keys;        /* 当前按住状态掩码 */
xdata unsigned char keys_prev;   /* 上一帧掩码 */
xdata unsigned char fire_edge;   /* 开火边沿 */
xdata unsigned int  send_tick;   /* 发送节拍 */

void cb_key(void) {
    unsigned char k;
    k = GetKeyAct(enumKey1);
    if (k == enumKeyPress) keys |= K_BACK;
    else if (k == enumKeyRelease) keys &= ~K_BACK;

    k = GetKeyAct(enumKey3);
    if (k == enumKeyPress) keys |= K_FWD;
    else if (k == enumKeyRelease) keys &= ~K_FWD;

    k = GetKeyAct(enumKey2);
    if (k == enumKeyPress) fire_edge = 1;
}

void cb_nav(void) {
    unsigned char k;
    k = GetAdcNavAct(enumAdcNavKeyDown);
    if (k == enumKeyPress) keys |= K_LEFT;
    else if (k == enumKeyRelease) keys &= ~K_LEFT;

    k = GetAdcNavAct(enumAdcNavKeyUp);
    if (k == enumKeyPress) keys |= K_RIGHT;
    else if (k == enumKeyRelease) keys &= ~K_RIGHT;
}

void send_keys(void) {
    unsigned char tx[4];
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
    tx[0] = HEAD_SLV0;
    tx[1] = HEAD_SLV1;
    tx[2] = mask;
    tx[3] = (unsigned char)(HEAD_SLV0 + HEAD_SLV1 + mask);
    Uart2Print(tx, 4);
    if (!changed) send_tick = 10;   /* 无变化时每 100ms 保底发送一次(维持在线指示) */
}

void cb_10ms(void) {
    if (send_tick > 0) send_tick--;
    send_keys();
}

void cb_led(void) {
    /* LED: 低5位直接反映按键状态, 便于调试 */
    LedPrint(keys);
}

void main(void) {
    DisplayerInit();
    KeyInit();
    AdcInit(ADCincEXT);
    Uart2Init(38400, Uart2Usedfor485);

    keys = 0; keys_prev = 0; fire_edge = 0;
    send_tick = 0;
    SetDisplayerArea(0, 7);
    Seg7Print(10, 10, 10, 10, 10, 10, 10, 10);
    LedPrint(0);
    Seg7Print(2, 10, 10, 10, 10, 10, 10, 10);  /* 显示 '2' 表示 Player 2 */

    SetEventCallBack(enumEventSys10mS, cb_10ms);
    SetEventCallBack(enumEventKey, cb_key);
    SetEventCallBack(enumEventNav, cb_nav);
    SetEventCallBack(enumEventSys100mS, cb_led);

    MySTC_Init();

    while (1) {
        MySTC_OS();
    }
}
