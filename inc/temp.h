#ifndef _TEMP_H_
#define _TEMP_H_
/* =====================================================================
 * NTC 热敏电阻 (10K/3950) ADC -> 温度换算 (共享于 Host 与 Slave)
 * 直接移植 hw8 项目: 查表 + 线性插值。
 *   temp_table 单位 0.1°C (即 300 = 30.0°C)
 *   温度越高, ADC 值越低。
 * ===================================================================== */

/* 温度值 (0.1°C), 对应 adc_table 各采样点 */
code int temp_table[19] = {
    -50, 0, 50, 100, 150, 200, 250, 300, 350, 400,
    450, 500, 550, 600, 650, 700, 750, 800, 850
};

/* 10bit ADC 采样点 (温度从低到高, ADC 递减) */
code unsigned int adc_table[19] = {
    834, 789, 739, 685, 628, 570, 512, 456, 404, 355,
    310, 270, 235, 204, 177, 153, 133, 115, 100
};

/* ADC -> 温度 (0.1°C), 线性插值; 越界取端点 */
int calc_temp(unsigned int adc) {
    unsigned char i;
    int a = (int)adc;
    if (a >= (int)adc_table[0])  return temp_table[0];
    if (a <= (int)adc_table[18]) return temp_table[18];
    for (i = 0; i < 18; i++) {
        if (a <= (int)adc_table[i] && a >= (int)adc_table[i + 1]) {
            return temp_table[i] +
                (int)((long)(a - (int)adc_table[i]) *
                      (temp_table[i + 1] - temp_table[i]) /
                      ((int)adc_table[i + 1] - (int)adc_table[i]));
        }
    }
    return temp_table[0];
}
#endif /* _TEMP_H_ */