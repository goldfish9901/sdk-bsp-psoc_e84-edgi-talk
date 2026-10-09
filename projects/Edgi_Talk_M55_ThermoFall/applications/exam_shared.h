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
    THERMOFALL_PAGE_THERMO = 0,
    THERMOFALL_PAGE_FALL   = 1
} thermofall_page_t;

typedef enum
{
    THERMO_COMFORT_GOOD = 0,      /* 舒适：绿灯慢闪 */
    THERMO_COMFORT_WARM,          /* 偏热或偏潮：红灯慢闪 */
    THERMO_COMFORT_ALARM,         /* 高温或高湿：红灯常亮 */
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
    rt_uint32_t magic;       /* 固定标记：说明温湿度缓存已经初始化 */
    rt_uint32_t sequence;    /* 每采样一次加 1，UI 用它显示刷新次数 */
    float temperature;       /* AHT20 温度，单位摄氏度 */
    float humidity;          /* AHT20 湿度，单位 %RH */
    rt_uint8_t paused;       /* 1 表示暂停温湿度采样 */
    rt_uint8_t alarm;        /* 1 表示超过温湿度报警阈值 */
    rt_uint8_t sensor_ok;    /* 1 表示 AHT20 初始化成功 */
    rt_uint8_t reserved;
} thermometer_shared_t;

typedef struct
{
    rt_uint32_t magic;      /* 固定标记：说明摔倒检测缓存已经初始化 */
    rt_uint32_t sequence;   /* 每次 LSM6DS3 采样加 1 */
    float ax;               /* 三轴加速度，单位 mg */
    float ay;
    float az;
    float gx;               /* 三轴角速度，单位 mdps */
    float gy;
    float gz;
    rt_uint8_t state;       /* FALLCHECK_STATE_* */
    rt_uint8_t alarm;       /* 1 表示摔倒报警 */
    rt_uint8_t muted;       /* 1 表示 Fall 页面静音 */
    rt_uint8_t sensor_ok;   /* 1 表示 LSM6DS3 初始化成功 */
} fallcheck_shared_t;

#define THERMOMETER_SHARED_MAGIC 0x54484D31u
#define FALLCHECK_SHARED_MAGIC   0x46414C4Cu

#define FALLCHECK_STATE_IDLE   0U
#define FALLCHECK_STATE_NORMAL 1U
#define FALLCHECK_STATE_FALL   2U

extern volatile thermometer_shared_t g_thermometer_shared;
extern volatile fallcheck_shared_t g_fallcheck_shared;
extern volatile thermofall_page_t g_thermofall_page;

#endif
