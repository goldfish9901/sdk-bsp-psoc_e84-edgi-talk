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

    /*
     * 以下为调试字段，由传感器线程每拍（100ms）更新，供 LVGL 调试视图显示。
     * 摔倒检测需要猛烈甩动开发板，串口根本没法看，所以把判据原样搬到屏幕上。
     * 状态机真正使用的就是 acc_mag 与 gyro_sum 这两个派生量，而不是单轴值。
     */
    float acc_mag;          /* sqrt(ax^2+ay^2+az^2)，单位 mg */
    float gyro_sum;         /* |gx|+|gy|+|gz|，单位 mdps */
    float acc_mag_peak;     /* acc_mag 峰值保持，甩完板子后仍能读到 */
    float gyro_sum_peak;    /* gyro_sum 峰值保持 */
    rt_uint32_t flags;      /* FALLCHECK_FLAG_* 位组合 */
    rt_uint8_t cnt_still;   /* 连续静止采样数 */
    rt_uint8_t cnt_confirm; /* 候选确认窗口剩余拍数 */
    rt_uint8_t cnt_hold;    /* FALL 状态保持剩余拍数 */
    rt_uint8_t cnt_motion;  /* 姿态变化判定窗口剩余拍数 */
    rt_uint8_t cnt_warmup;  /* 预热计数，未满前不做摔倒判定 */
    rt_uint8_t dbg_reset;   /* UI 写 1 请求清零峰值，传感器线程清零后置回 0 */
} fallcheck_shared_t;

/* flags 位定义：与状态机里的判据一一对应 */
#define FALLCHECK_FLAG_STILL          (1u << 0)  /* 静止判据成立（加速度+陀螺仪都安静） */
#define FALLCHECK_FLAG_IMPACT         (1u << 1)  /* 强冲击：幅值超过 1700 mg */
#define FALLCHECK_FLAG_DROP           (1u << 2)  /* 短暂失重：幅值掉到 350~700 mg 且有转动 */
#define FALLCHECK_FLAG_FAST_ROTATE    (1u << 3)  /* 快速旋转：gyro_sum 超过 650000 mdps */
#define FALLCHECK_FLAG_IMPACT_ROTATE  (1u << 4)  /* 冲击伴随旋转 */
#define FALLCHECK_FLAG_POSTURE        (1u << 5)  /* 重力方向大幅变化 */
#define FALLCHECK_FLAG_UPRIGHT        (1u << 6)  /* 标准直立（按安全落点处理，不确认摔倒） */

#define THERMOMETER_SHARED_MAGIC 0x54484D31u
#define FALLCHECK_SHARED_MAGIC   0x46414C4Cu

#define FALLCHECK_STATE_IDLE   0U
#define FALLCHECK_STATE_NORMAL 1U
#define FALLCHECK_STATE_FALL   2U

/*
 * 摔倒判定的调参常量。放在这里是为了让 LVGL 调试视图能直接引用同一份数值，
 * 而不是在界面代码里再抄一遍（抄两份必然会对不上）。
 * packages/lsm6ds3tr/lsm6ds3tr-c_port.c 直接使用这些宏。
 */
#define FALLCHECK_SAMPLE_MS              100U    /* 采样周期 */
#define FALLCHECK_STILL_MIN_SAMPLES      4U      /* 判定“静止”所需连续拍数 */
#define FALLCHECK_CONFIRM_WINDOW         12U     /* 候选确认窗口 */
#define FALLCHECK_FALL_HOLD_SAMPLES      50U     /* FALL 状态保持拍数（约 5s） */
#define FALLCHECK_WARMUP_SAMPLES         10U     /* 预热，未满不做摔倒判定 */
#define FALLCHECK_POSTURE_MOTION_SAMPLES 8U     /* 姿态变化判定窗口 */
#define FALLCHECK_VALID_ACC_MIN_MG       500.0f  /* 幅值低于此值认为数据无效 */
#define FALLCHECK_IDLE_ACC_LOW_MG        800.0f  /* 静止幅值下限 */
#define FALLCHECK_IDLE_ACC_HIGH_MG       1200.0f /* 静止幅值上限 */
#define FALLCHECK_IDLE_GYRO_SUM_MDPS     60000.0f
#define FALLCHECK_IMPACT_HIGH_MG         1700.0f /* 强冲击阈值 */
#define FALLCHECK_IMPACT_LOW_MG          350.0f  /* 失重下限 */
#define FALLCHECK_DROP_ACC_HIGH_MG       700.0f  /* 失重上限 */
#define FALLCHECK_DROP_GYRO_SUM_MDPS     20000.0f
#define FALLCHECK_ROTATE_SUM_MDPS        650000.0f
#define FALLCHECK_ROTATE_WITH_IMPACT     250000.0f
#define FALLCHECK_POSTURE_CHANGE_COS     0.5736f /* cos(55°) */
#define FALLCHECK_UPRIGHT_X_MIN_MG       850.0f
#define FALLCHECK_UPRIGHT_SIDE_MAX_MG    250.0f

extern volatile thermometer_shared_t g_thermometer_shared;
extern volatile fallcheck_shared_t g_fallcheck_shared;
extern volatile thermofall_page_t g_thermofall_page;

#endif
