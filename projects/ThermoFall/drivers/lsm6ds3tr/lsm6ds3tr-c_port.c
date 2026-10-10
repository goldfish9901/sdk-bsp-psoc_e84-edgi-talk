/* Includes ------------------------------------------------------------------*/
#include "lsm6ds3tr-c_reg.h"
#include <string.h>
#include <stdio.h>
#include <rtthread.h>
#include <rtdevice.h>
#include "../../applications/exam_shared.h"

volatile fallcheck_shared_t g_fallcheck_shared __attribute__((section(".m33_m55_shared_hyperram"), used)) =
{
    FALLCHECK_SHARED_MAGIC,
    FALLCHECK_STATE_IDLE,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0,
    0,
    0,
    0
};

/* 私有宏定义 ----------------------------------------------------------------*/
#define    LSM6DS3_I2C_BUS_NAME "i2c0"
#define FALLCHECK_SAMPLE_MS             100U
#define FALLCHECK_LOG_DIVIDER           5U
#define FALLCHECK_STILL_MIN_SAMPLES     4U
#define FALLCHECK_CONFIRM_WINDOW        12U
#define FALLCHECK_FALL_HOLD_SAMPLES     50U
#define FALLCHECK_WARMUP_SAMPLES        10U
#define FALLCHECK_VALID_ACC_MIN_MG      500.0f
#define FALLCHECK_IDLE_ACC_LOW_MG       800.0f
#define FALLCHECK_IDLE_ACC_HIGH_MG      1200.0f
#define FALLCHECK_IDLE_GYRO_SUM_MDPS    60000.0f
#define FALLCHECK_IMPACT_HIGH_MG        1700.0f
#define FALLCHECK_IMPACT_LOW_MG         350.0f
#define FALLCHECK_DROP_ACC_HIGH_MG      700.0f
#define FALLCHECK_DROP_GYRO_SUM_MDPS    20000.0f
#define FALLCHECK_ROTATE_SUM_MDPS       650000.0f
#define FALLCHECK_ROTATE_WITH_IMPACT    250000.0f
#define FALLCHECK_POSTURE_CHANGE_COS    0.5736f
#define FALLCHECK_POSTURE_MOTION_SAMPLES 8U
#define FALLCHECK_UPRIGHT_X_MIN_MG      850.0f
#define FALLCHECK_UPRIGHT_SIDE_MAX_MG   250.0f
/* 私有变量 ------------------------------------------------------------------*/
static int16_t data_raw_acceleration[3];
static int16_t data_raw_angular_rate[3];
static int16_t data_raw_temperature;
static float_t acceleration_mg[3];
static float_t angular_rate_mdps[3];
static float_t temperature_degC;
static uint8_t whoamI, rst;
static struct rt_i2c_bus_device *i2c_bus = RT_NULL;

/* 外部变量 ------------------------------------------------------------------*/

/* 私有函数声明 --------------------------------------------------------------*/

/*
 *   WARNING:
 *   Functions declare in this section are defined at the end of this file
 *   and are strictly related to the hardware platform used.
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, uint8_t *bufp,
                              uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);
static void platform_delay(uint32_t ms);
static void platform_init(void);

static float fallcheck_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static rt_bool_t fallcheck_is_standard_upright(float ax, float ay, float az, float gyro_sum)
{
    /*
     * 手动把开发板快速扶回标准直立姿态时：
     * X 轴接近 +1g，Y/Z 轴接近 0，并且陀螺仪已经安静。
     * 这种情况按“安全落点”处理，不作为摔倒确认。
     */
    return (ax > FALLCHECK_UPRIGHT_X_MIN_MG &&
            fallcheck_absf(ay) < FALLCHECK_UPRIGHT_SIDE_MAX_MG &&
            fallcheck_absf(az) < FALLCHECK_UPRIGHT_SIDE_MAX_MG &&
            gyro_sum < FALLCHECK_IDLE_GYRO_SUM_MDPS) ? RT_TRUE : RT_FALSE;
}

static rt_uint8_t fallcheck_state_from_axes(float ax, float ay, float az, float gx, float gy, float gz)
{
    static rt_uint8_t still_samples = 0;
    static rt_uint8_t confirm_window = 0;
    static rt_uint8_t fall_hold = 0;
    static rt_uint8_t warmup_samples = 0;
    static rt_uint8_t motion_samples = 0;
    static rt_uint8_t has_last_still = 0;
    static float last_still_ax = 0.0f;
    static float last_still_ay = 0.0f;
    static float last_still_az = 0.0f;
    static float last_still_mag2 = 0.0f;

    float acc_mag2 = ax * ax + ay * ay + az * az;
    float gyro_sum = fallcheck_absf(gx) + fallcheck_absf(gy) + fallcheck_absf(gz);
    rt_bool_t valid_acc = (acc_mag2 > (FALLCHECK_VALID_ACC_MIN_MG * FALLCHECK_VALID_ACC_MIN_MG));
    rt_bool_t acc_still = (acc_mag2 >= (FALLCHECK_IDLE_ACC_LOW_MG * FALLCHECK_IDLE_ACC_LOW_MG)) &&
                          (acc_mag2 <= (FALLCHECK_IDLE_ACC_HIGH_MG * FALLCHECK_IDLE_ACC_HIGH_MG));
    rt_bool_t gyro_still = (gyro_sum < FALLCHECK_IDLE_GYRO_SUM_MDPS);
    rt_bool_t still = (valid_acc && acc_still && gyro_still) ? RT_TRUE : RT_FALSE;
    rt_bool_t impact_high = (acc_mag2 > (FALLCHECK_IMPACT_HIGH_MG * FALLCHECK_IMPACT_HIGH_MG));
    rt_bool_t drop_dip = (acc_mag2 > (FALLCHECK_IMPACT_LOW_MG * FALLCHECK_IMPACT_LOW_MG)) &&
                         (acc_mag2 < (FALLCHECK_DROP_ACC_HIGH_MG * FALLCHECK_DROP_ACC_HIGH_MG)) &&
                         (gyro_sum > FALLCHECK_DROP_GYRO_SUM_MDPS);
    rt_bool_t fast_rotate = (gyro_sum > FALLCHECK_ROTATE_SUM_MDPS);
    rt_bool_t impact_rotate = (impact_high && (gyro_sum > FALLCHECK_ROTATE_WITH_IMPACT));
    rt_bool_t standard_upright = fallcheck_is_standard_upright(ax, ay, az, gyro_sum);
    rt_bool_t posture_change = RT_FALSE;

    if (warmup_samples < FALLCHECK_WARMUP_SAMPLES)
    {
        warmup_samples++;
    }

    if (!valid_acc)
    {
        still_samples = 0;
        confirm_window = 0;
        return FALLCHECK_STATE_NORMAL;
    }

    if (!still || gyro_sum > FALLCHECK_DROP_GYRO_SUM_MDPS)
    {
        motion_samples = FALLCHECK_POSTURE_MOTION_SAMPLES;
    }

    if (still)
    {
        if (still_samples < 255U)
        {
            still_samples++;
        }

        if (has_last_still && motion_samples > 0U)
        {
            float dot = ax * last_still_ax + ay * last_still_ay + az * last_still_az;
            float limit2 = acc_mag2 * last_still_mag2 * FALLCHECK_POSTURE_CHANGE_COS * FALLCHECK_POSTURE_CHANGE_COS;

            if (dot <= 0.0f || (dot * dot) < limit2)
            {
                posture_change = RT_TRUE;
            }
        }
    }
    else
    {
        still_samples = 0;
    }

    if (fall_hold > 0U)
    {
        fall_hold--;
        return FALLCHECK_STATE_FALL;
    }

    /*
     * 穿戴式摔倒检测规则：
     * 1. 只有出现明显冲击、短暂失重或非常快速旋转时，才进入候选窗口；
     * 2. 运动后如果重力方向发生大幅变化，也进入候选窗口；
     * 3. 候选窗口内设备很快重新静止，才确认 FALL。
     * 这样可以减少普通手摇、轻微晃动带来的误报。
     */
    if (warmup_samples >= FALLCHECK_WARMUP_SAMPLES &&
        (drop_dip || impact_rotate || (impact_high && fast_rotate) || posture_change))
    {
        confirm_window = FALLCHECK_CONFIRM_WINDOW;
    }

    if (confirm_window > 0U)
    {
        confirm_window--;
        if (still && standard_upright)
        {
            confirm_window = 0;
            fall_hold = 0;
            last_still_ax = ax;
            last_still_ay = ay;
            last_still_az = az;
            last_still_mag2 = acc_mag2;
            has_last_still = 1;
            return (still_samples >= FALLCHECK_STILL_MIN_SAMPLES) ? FALLCHECK_STATE_IDLE : FALLCHECK_STATE_NORMAL;
        }

        if (still_samples >= FALLCHECK_STILL_MIN_SAMPLES)
        {
            fall_hold = FALLCHECK_FALL_HOLD_SAMPLES;
            confirm_window = 0;
            return FALLCHECK_STATE_FALL;
        }
        return FALLCHECK_STATE_NORMAL;
    }

    if (still)
    {
        last_still_ax = ax;
        last_still_ay = ay;
        last_still_az = az;
        last_still_mag2 = acc_mag2;
        has_last_still = 1;
    }

    if (motion_samples > 0U)
    {
        motion_samples--;
    }

    return (still_samples >= FALLCHECK_STILL_MIN_SAMPLES) ? FALLCHECK_STATE_IDLE : FALLCHECK_STATE_NORMAL;
}

static void fallcheck_sensor_thread(void *parameter)
{
    (void)parameter;

    /* 初始化 MEMS 驱动接口 */
    stmdev_ctx_t dev_ctx;
    dev_ctx.write_reg = platform_write;
    dev_ctx.read_reg = platform_read;
    dev_ctx.mdelay = platform_delay;
    /* 初始化测试平台 */
    platform_init();
    if (i2c_bus == RT_NULL)
    {
        return;
    }

    /* 检查设备 ID */
    whoamI = 0;
    lsm6ds3tr_c_device_id_get(&dev_ctx, &whoamI);

    if (whoamI != LSM6DS3TR_C_ID)
    {
        rt_kprintf("[FallCheck] LSM6DS3 not found\r\n");
        g_fallcheck_shared.sensor_ok = 0;
        return;
    }

    /* 恢复默认配置 */
    lsm6ds3tr_c_reset_set(&dev_ctx, PROPERTY_ENABLE);

    do
    {
        lsm6ds3tr_c_reset_get(&dev_ctx, &rst);
    }
    while (rst);

    /* 开启块数据更新，避免读到新旧混合数据 */
    lsm6ds3tr_c_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
    /* 设置输出数据率 */
    lsm6ds3tr_c_xl_data_rate_set(&dev_ctx, LSM6DS3TR_C_XL_ODR_52Hz);
    lsm6ds3tr_c_gy_data_rate_set(&dev_ctx, LSM6DS3TR_C_GY_ODR_52Hz);
    /* 设置量程 */
    lsm6ds3tr_c_xl_full_scale_set(&dev_ctx, LSM6DS3TR_C_2g);
    lsm6ds3tr_c_gy_full_scale_set(&dev_ctx, LSM6DS3TR_C_2000dps);
    /* 配置滤波链路，不使用辅助接口 */
    lsm6ds3tr_c_xl_filter_analog_set(&dev_ctx, LSM6DS3TR_C_XL_ANA_BW_400Hz);
    lsm6ds3tr_c_xl_lp2_bandwidth_set(&dev_ctx, LSM6DS3TR_C_XL_LOW_NOISE_LP_ODR_DIV_100);
    lsm6ds3tr_c_gy_band_pass_set(&dev_ctx, LSM6DS3TR_C_HP_260mHz_LP1_STRONG);

    g_fallcheck_shared.sensor_ok = 1;
    rt_kprintf("[FallCheck] LSM6DS3 ready\r\n");

    /* 使用轮询方式读取样本，不依赖中断 */
    while (1)
    {
        lsm6ds3tr_c_reg_t reg;
        reg.status_reg.xlda = 0;
        reg.status_reg.gda = 0;
        reg.status_reg.tda = 0;
        lsm6ds3tr_c_status_reg_get(&dev_ctx, &reg.status_reg);
        if (reg.status_reg.xlda)
        {
            rt_memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
            lsm6ds3tr_c_acceleration_raw_get(&dev_ctx, data_raw_acceleration);
            acceleration_mg[0] = lsm6ds3tr_c_from_fs2g_to_mg(data_raw_acceleration[0]);
            acceleration_mg[1] = lsm6ds3tr_c_from_fs2g_to_mg(data_raw_acceleration[1]);
            acceleration_mg[2] = lsm6ds3tr_c_from_fs2g_to_mg(data_raw_acceleration[2]);
        }

        if (reg.status_reg.gda)
        {
            rt_memset(data_raw_angular_rate, 0x00, 3 * sizeof(int16_t));
            lsm6ds3tr_c_angular_rate_raw_get(&dev_ctx, data_raw_angular_rate);
            angular_rate_mdps[0] = lsm6ds3tr_c_from_fs2000dps_to_mdps(data_raw_angular_rate[0]);
            angular_rate_mdps[1] = lsm6ds3tr_c_from_fs2000dps_to_mdps(data_raw_angular_rate[1]);
            angular_rate_mdps[2] = lsm6ds3tr_c_from_fs2000dps_to_mdps(data_raw_angular_rate[2]);
        }

        if (reg.status_reg.tda)
        {
            rt_memset(&data_raw_temperature, 0x00, sizeof(int16_t));
            lsm6ds3tr_c_temperature_raw_get(&dev_ctx, &data_raw_temperature);
            temperature_degC = lsm6ds3tr_c_from_lsb_to_celsius(data_raw_temperature);
        }

        g_fallcheck_shared.ax = acceleration_mg[0];
        g_fallcheck_shared.ay = acceleration_mg[1];
        g_fallcheck_shared.az = acceleration_mg[2];
        g_fallcheck_shared.gx = angular_rate_mdps[0];
        g_fallcheck_shared.gy = angular_rate_mdps[1];
        g_fallcheck_shared.gz = angular_rate_mdps[2];
        g_fallcheck_shared.state = fallcheck_state_from_axes(
                                       g_fallcheck_shared.ax,
                                       g_fallcheck_shared.ay,
                                       g_fallcheck_shared.az,
                                       g_fallcheck_shared.gx,
                                       g_fallcheck_shared.gy,
                                       g_fallcheck_shared.gz);
        g_fallcheck_shared.alarm = (g_fallcheck_shared.state == FALLCHECK_STATE_FALL && !g_fallcheck_shared.muted) ? 1 : 0;
        g_fallcheck_shared.sequence++;

        if ((g_fallcheck_shared.sequence % FALLCHECK_LOG_DIVIDER) == 0U)
        {
            rt_kprintf("[FallCheck] acc=%d.%02d,%d.%02d,%d.%02d gyro=%d.%02d,%d.%02d,%d.%02d state=%u mute=%u seq=%u\r\n",
                       (int)g_fallcheck_shared.ax, (int)(fallcheck_absf(g_fallcheck_shared.ax) * 100) % 100,
                       (int)g_fallcheck_shared.ay, (int)(fallcheck_absf(g_fallcheck_shared.ay) * 100) % 100,
                       (int)g_fallcheck_shared.az, (int)(fallcheck_absf(g_fallcheck_shared.az) * 100) % 100,
                       (int)g_fallcheck_shared.gx, (int)(fallcheck_absf(g_fallcheck_shared.gx) * 100) % 100,
                       (int)g_fallcheck_shared.gy, (int)(fallcheck_absf(g_fallcheck_shared.gy) * 100) % 100,
                       (int)g_fallcheck_shared.gz, (int)(fallcheck_absf(g_fallcheck_shared.gz) * 100) % 100,
                       g_fallcheck_shared.state, g_fallcheck_shared.muted, g_fallcheck_shared.sequence);
        }

        rt_thread_mdelay(FALLCHECK_SAMPLE_MS);
    }
}

/* 主示例入口 ----------------------------------------------------------------*/
int lsm6ds3tr_c_read_data_sample(void)
{
    rt_thread_t tid;

    g_fallcheck_shared.magic = FALLCHECK_SHARED_MAGIC;
    g_fallcheck_shared.state = FALLCHECK_STATE_IDLE;
    g_fallcheck_shared.ax = 0.0f;
    g_fallcheck_shared.ay = 0.0f;
    g_fallcheck_shared.az = 0.0f;
    g_fallcheck_shared.gx = 0.0f;
    g_fallcheck_shared.gy = 0.0f;
    g_fallcheck_shared.gz = 0.0f;
    g_fallcheck_shared.alarm = 0;
    g_fallcheck_shared.muted = 0;
    g_fallcheck_shared.sensor_ok = 0;
    g_fallcheck_shared.sequence = 0;

    tid = rt_thread_create("fall_sns", fallcheck_sensor_thread, RT_NULL, 2048, 20, 20);
    if (tid == RT_NULL)
    {
        rt_kprintf("[FallCheck] sensor thread create failed\r\n");
        return -RT_ERROR;
    }

    rt_thread_startup(tid);
    return RT_EOK;
}
INIT_APP_EXPORT(lsm6ds3tr_c_read_data_sample);

/*
 * @brief  写入通用设备寄存器，和当前平台相关
 *
 * @param  handle    可自定义参数，本工程中不使用该参数
 * @param  reg       要写入的寄存器地址
 * @param  bufp      要写入寄存器的数据缓冲区
 * @param  len       连续写入的寄存器数据长度
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, uint8_t *bufp,
                              uint16_t len)
{
    struct rt_i2c_msg msg;
    rt_uint8_t buf[1 + len];

    buf[0] = reg;
    rt_memcpy(&buf[1], bufp, len);

    msg.addr  = LSM6DS3TR_C_ID;
    msg.flags = RT_I2C_WR;
    msg.buf   = buf;
    msg.len   = len + 1;

    if (rt_i2c_transfer(i2c_bus, &msg, 1) == 1)
    {
        return RT_EOK;
    }
    else
    {
        return -RT_ERROR;
    }
}


/*
 * @brief  读取通用设备寄存器，和当前平台相关
 *
 * @param  handle    可自定义参数，本工程中不使用该参数
 * @param  reg       要读取的寄存器地址
 * @param  bufp      保存读取数据的缓冲区
 * @param  len       连续读取的寄存器数据长度
 *
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr = LSM6DS3TR_C_ID;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = LSM6DS3TR_C_ID;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = bufp;
    msgs[1].len = len;

    if (rt_i2c_transfer(i2c_bus, msgs, 2) == 2)
    {
        return RT_EOK;
    }
    else
    {
        return -RT_ERROR;
    }
}

/*
 * @brief  平台相关延时函数
 *
 * @param  ms        延时时间，单位 ms
 *
 */
static void platform_delay(uint32_t ms)
{
    rt_thread_mdelay(ms);
}

/*
 * @brief  平台相关初始化函数，查找 LSM6DS3 使用的 I2C 总线
 */
static void platform_init(void)
{
    i2c_bus = (struct rt_i2c_bus_device *)rt_device_find(LSM6DS3_I2C_BUS_NAME);
    if (i2c_bus == RT_NULL)
    {
        rt_kprintf("Error: I2C bus %s not found!\n", LSM6DS3_I2C_BUS_NAME);
    }
}
