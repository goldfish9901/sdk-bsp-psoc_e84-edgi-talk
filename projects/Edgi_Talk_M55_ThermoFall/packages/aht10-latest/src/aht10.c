/*
 * AHT20 driver adapted from applications/care_bot/care_aht20.c.
 * Keep the original aht10 package API so the exam application code stays simple.
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>

#define DBG_TAG "aht20"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "aht10.h"

#ifdef PKG_USING_AHT10

#define AHT20_ADDR              0x38
#define AHT20_STATUS_BUSY       0x80
#define AHT20_STATUS_CALIB      0x08
#define AHT20_CMD_INIT          0xBE
#define AHT20_CMD_TRIGGER       0xAC
#define AHT20_CMD_SOFT_RESET    0xBA
#define AHT20_CACHE_MS          200

static float g_last_temperature = -50.0f;
static float g_last_humidity = 0.0f;
static rt_tick_t g_last_tick = 0;
static rt_bool_t g_cache_valid = RT_FALSE;

static rt_err_t aht20_send3(struct rt_i2c_bus_device *bus, rt_uint8_t cmd, rt_uint8_t arg0, rt_uint8_t arg1)
{
    rt_uint8_t packet[3];

    packet[0] = cmd;
    packet[1] = arg0;
    packet[2] = arg1;

    return (rt_i2c_master_send(bus, AHT20_ADDR, 0, packet, sizeof(packet)) == (rt_ssize_t)sizeof(packet)) ? RT_EOK : -RT_ERROR;
}

static rt_err_t aht20_send1(struct rt_i2c_bus_device *bus, rt_uint8_t cmd)
{
    return (rt_i2c_master_send(bus, AHT20_ADDR, 0, &cmd, 1) == 1) ? RT_EOK : -RT_ERROR;
}

static rt_err_t aht20_read(struct rt_i2c_bus_device *bus, rt_uint8_t *data, rt_uint32_t len)
{
    return (rt_i2c_master_recv(bus, AHT20_ADDR, 0, data, len) == (rt_ssize_t)len) ? RT_EOK : -RT_ERROR;
}

static rt_bool_t aht20_decode_measurement(const rt_uint8_t data[6], float *temperature, float *humidity)
{
    rt_uint32_t humidity_raw;
    rt_uint32_t temp_raw;

    if ((data[0] & AHT20_STATUS_BUSY) != 0U)
    {
        return RT_FALSE;
    }

    humidity_raw = ((rt_uint32_t)data[1] << 12) |
                   ((rt_uint32_t)data[2] << 4) |
                   (((rt_uint32_t)data[3] & 0xF0U) >> 4);
    temp_raw = (((rt_uint32_t)data[3] & 0x0FU) << 16) |
               ((rt_uint32_t)data[4] << 8) |
               (rt_uint32_t)data[5];

    *humidity = (float)humidity_raw * 100.0f / 1048576.0f;
    if (*humidity > 100.0f)
    {
        *humidity = 100.0f;
    }
    *temperature = (float)temp_raw * 200.0f / 1048576.0f - 50.0f;

    return RT_TRUE;
}

static rt_bool_t aht20_read_measurement(aht10_device_t dev, float *temperature, float *humidity)
{
    rt_uint8_t status = 0;
    rt_uint8_t data[6] = {0};
    rt_err_t result;

    RT_ASSERT(dev);

    /* 温湿度在 main.c 中会连续读取两次，这里用短缓存避免重复触发测量。 */
    if (g_cache_valid && (rt_tick_get() - g_last_tick) < rt_tick_from_millisecond(AHT20_CACHE_MS))
    {
        *temperature = g_last_temperature;
        *humidity = g_last_humidity;
        return RT_TRUE;
    }

    result = rt_mutex_take(dev->lock, RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        LOG_W("AHT20 mutex take failed");
        return RT_FALSE;
    }

    /* 完全参考 care_aht20.c：初始化失败时先软复位，再重试一次。 */
    if (aht20_send3(dev->i2c, AHT20_CMD_INIT, 0x08, 0x00) != RT_EOK)
    {
        aht20_send1(dev->i2c, AHT20_CMD_SOFT_RESET);
        rt_thread_mdelay(20);
        if (aht20_send3(dev->i2c, AHT20_CMD_INIT, 0x08, 0x00) != RT_EOK)
        {
            LOG_W("AHT20 init failed on %s", PKG_AHT10_I2C_BUS_NAME);
            rt_mutex_release(dev->lock);
            return RT_FALSE;
        }
    }
    rt_thread_mdelay(10);

    if (aht20_read(dev->i2c, &status, 1) != RT_EOK)
    {
        LOG_W("AHT20 status read failed");
        rt_mutex_release(dev->lock);
        return RT_FALSE;
    }

    if ((status & AHT20_STATUS_CALIB) == 0U)
    {
        LOG_W("AHT20 is not calibrated, status=0x%02x", status);
        rt_mutex_release(dev->lock);
        return RT_FALSE;
    }

    if (aht20_send3(dev->i2c, AHT20_CMD_TRIGGER, 0x33, 0x00) != RT_EOK)
    {
        LOG_W("AHT20 measure command failed");
        rt_mutex_release(dev->lock);
        return RT_FALSE;
    }
    rt_thread_mdelay(80);

    if (aht20_read(dev->i2c, data, sizeof(data)) != RT_EOK)
    {
        LOG_W("AHT20 measurement read failed");
        rt_mutex_release(dev->lock);
        return RT_FALSE;
    }

    rt_mutex_release(dev->lock);

    if (!aht20_decode_measurement(data, temperature, humidity))
    {
        LOG_W("AHT20 decode failed, status=0x%02x", data[0]);
        return RT_FALSE;
    }

    g_last_temperature = *temperature;
    g_last_humidity = *humidity;
    g_last_tick = rt_tick_get();
    g_cache_valid = RT_TRUE;
    return RT_TRUE;
}

float aht10_read_temperature(aht10_device_t dev)
{
    float temperature = -50.0f;
    float humidity = 0.0f;

    if (aht20_read_measurement(dev, &temperature, &humidity))
    {
        return temperature;
    }

    return -50.0f;
}

float aht10_read_humidity(aht10_device_t dev)
{
    float temperature = -50.0f;
    float humidity = 0.0f;

    if (aht20_read_measurement(dev, &temperature, &humidity))
    {
        return humidity;
    }

    return 0.0f;
}

aht10_device_t aht10_init(const char *i2c_bus_name)
{
    aht10_device_t dev;

    RT_ASSERT(i2c_bus_name);

    dev = rt_calloc(1, sizeof(struct aht10_device));
    if (dev == RT_NULL)
    {
        LOG_E("Can't allocate memory for AHT20 device on '%s'", i2c_bus_name);
        return RT_NULL;
    }

    dev->i2c = rt_i2c_bus_device_find(i2c_bus_name);
    if (dev->i2c == RT_NULL)
    {
        LOG_E("Can't find AHT20 I2C bus '%s'", i2c_bus_name);
        rt_free(dev);
        return RT_NULL;
    }

    dev->lock = rt_mutex_create("mutex_aht20", RT_IPC_FLAG_FIFO);
    if (dev->lock == RT_NULL)
    {
        LOG_E("Can't create mutex for AHT20 device on '%s'", i2c_bus_name);
        rt_free(dev);
        return RT_NULL;
    }

    LOG_I("AHT20 bus ready on %s", i2c_bus_name);
    return dev;
}

void aht10_deinit(aht10_device_t dev)
{
    RT_ASSERT(dev);

    rt_mutex_delete(dev->lock);
    rt_free(dev);
}

#endif /* PKG_USING_AHT10 */