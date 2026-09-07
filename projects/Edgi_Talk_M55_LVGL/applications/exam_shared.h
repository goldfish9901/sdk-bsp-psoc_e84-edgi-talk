#ifndef EXAM_SHARED_H__
#define EXAM_SHARED_H__

#include <rtthread.h>

#define THERMO_TEMP_WARM_C        28.0f
#define THERMO_HUMI_WARM_PERCENT  70.0f
#define THERMO_TEMP_ALARM_C       35.0f
#define THERMO_HUMI_ALARM_PERCENT 80.0f
#define THERMO_TEMP_COOL_C        16.0f
#define THERMO_HUMI_DRY_PERCENT   30.0f

typedef enum
{
    THERMO_COMFORT_GOOD = 0,      /* 温湿度舒适：绿灯慢闪 */
    THERMO_COMFORT_WARM,          /* 偏热或偏潮：红灯慢闪 */
    THERMO_COMFORT_ALARM,         /* 高温或高湿报警：红灯常亮 */
    THERMO_COMFORT_DRY_COOL       /* 偏干或偏冷：蓝灯慢闪 */
} thermo_comfort_level_t;

static inline thermo_comfort_level_t thermo_get_comfort_level(float temp, float humi)
{
    if (temp >= THERMO_TEMP_ALARM_C || humi >= THERMO_HUMI_ALARM_PERCENT)
    {
        return THERMO_COMFORT_ALARM;
    }
    if (temp >= THERMO_TEMP_WARM_C || humi >= THERMO_HUMI_WARM_PERCENT)
    {
        return THERMO_COMFORT_WARM;
    }
    if (temp < THERMO_TEMP_COOL_C || humi < THERMO_HUMI_DRY_PERCENT)
    {
        return THERMO_COMFORT_DRY_COOL;
    }

    return THERMO_COMFORT_GOOD;
}
typedef struct
{
    rt_uint32_t magic;       /* 固定标记：说明本缓存已经初始化 */
    rt_uint32_t sequence;    /* 每采样一次加 1，UI 用它显示刷新次数 */
    float temperature;       /* AHT20 温度，单位摄氏度 */
    float humidity;          /* AHT20 湿度，单位 %RH */
    rt_uint8_t paused;       /* 按键或触摸切换：1 表示暂停采样/刷新 */
    rt_uint8_t alarm;        /* 1 表示超过阈值，可用于 LED 或 UI 告警 */
    rt_uint8_t sensor_ok;    /* 1 表示 AHT20 初始化成功 */
    rt_uint8_t reserved;
} thermometer_shared_t;

#define THERMOMETER_SHARED_MAGIC 0x54484D31u

extern volatile thermometer_shared_t g_thermometer_shared;

#endif
