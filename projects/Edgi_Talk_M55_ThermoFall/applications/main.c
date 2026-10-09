#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <dfs_fs.h>
#include <dfs_romfs.h>
#include <wavplayer.h>
#include <stdio.h>
#include <lv_rt_thread_conf.h>
#include "lv_port_disp.h"
#include "aht10.h"
#include "exam_shared.h"

/* PKG_AHT10_I2C_BUS_NAME 在 Kconfig 中位于 PKG_USING_AHT10_SAMPLE 之下（默认关闭），
 * 一旦走 menuconfig 重新生成 rtconfig.h 就会丢失，这里兜底保证 AHT20 仍挂在软 I2C2 上。 */
#ifndef PKG_AHT10_I2C_BUS_NAME
#define PKG_AHT10_I2C_BUS_NAME       "i2c2"
#endif

#define LED_PIN_B                    GET_PIN(16, 5)
#define LED_PIN_G                    GET_PIN(16, 6)
#define LED_PIN_R                    GET_PIN(16, 7)
#define LED_ON_LEVEL                 PIN_HIGH
#define LED_OFF_LEVEL                PIN_LOW
#define BUTTON_PIN                   GET_PIN(8, 3)

#define THERMO_SAMPLE_PERIOD_MS      1000
#define THERMO_LED_BLINK_MS          1000
#define LED_REFRESH_MS               50
#define ALARM_POLL_MS                100
#define ALARM_BEEP_COOLDOWN_MS       2500
#define FALLCHECK_BEEP_PATH          "/webnet/beep.wav"

extern const struct romfs_dirent romfs_root;

volatile thermometer_shared_t g_thermometer_shared =
{
    THERMOMETER_SHARED_MAGIC,
    0,
    0.0f,
    0.0f,
    0,
    0,
    0,
    0
};

volatile thermofall_page_t g_thermofall_page = THERMOFALL_PAGE_THERMO;

static aht10_device_t g_aht10_dev = RT_NULL;

void ui_thermofall_init(void);

void lv_user_gui_init(void)
{
    ui_thermofall_init();
}

static float app_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static void led_all_off(void)
{
    rt_pin_write(LED_PIN_R, LED_OFF_LEVEL);
    rt_pin_write(LED_PIN_G, LED_OFF_LEVEL);
    rt_pin_write(LED_PIN_B, LED_OFF_LEVEL);
}

static void thermometer_shared_reset(void)
{
    g_thermometer_shared.magic = THERMOMETER_SHARED_MAGIC;
    g_thermometer_shared.sequence = 0;
    g_thermometer_shared.temperature = 0.0f;
    g_thermometer_shared.humidity = 0.0f;
    g_thermometer_shared.paused = 0;
    g_thermometer_shared.alarm = 0;
    g_thermometer_shared.sensor_ok = 0;
    g_thermometer_shared.reserved = 0;
}

static void key_callback(void *args)
{
    (void)args;

    /* 实体 KEY 跟随当前页面：温湿度页暂停采样，Fall 页切换静音。 */
    if (g_thermofall_page == THERMOFALL_PAGE_FALL)
    {
        g_fallcheck_shared.muted = !g_fallcheck_shared.muted;
        rt_kprintf("[ThermoFall] fall muted=%d\r\n", g_fallcheck_shared.muted);
    }
    else
    {
        g_thermometer_shared.paused = !g_thermometer_shared.paused;
        rt_kprintf("[ThermoFall] thermo paused=%d\r\n", g_thermometer_shared.paused);
    }
}

static void thermo_led_update(void)
{
    rt_tick_t now = rt_tick_get();
    rt_uint8_t blink_on = ((now / rt_tick_from_millisecond(THERMO_LED_BLINK_MS)) % 2) ? 1 : 0;
    thermo_comfort_level_t level;

    level = thermo_get_comfort_level(g_thermometer_shared.temperature,
                                     g_thermometer_shared.humidity);

    led_all_off();

    switch (level)
    {
    case THERMO_COMFORT_ALARM:
        rt_pin_write(LED_PIN_R, LED_ON_LEVEL);
        break;
    case THERMO_COMFORT_WARM:
        rt_pin_write(LED_PIN_R, blink_on ? LED_ON_LEVEL : LED_OFF_LEVEL);
        break;
    case THERMO_COMFORT_DRY_COOL:
        rt_pin_write(LED_PIN_B, blink_on ? LED_ON_LEVEL : LED_OFF_LEVEL);
        break;
    case THERMO_COMFORT_GOOD:
    default:
        rt_pin_write(LED_PIN_G, blink_on ? LED_ON_LEVEL : LED_OFF_LEVEL);
        break;
    }
}

static void fall_led_update(void)
{
    led_all_off();

    if (g_fallcheck_shared.state == FALLCHECK_STATE_FALL)
    {
        rt_pin_write(LED_PIN_R, LED_ON_LEVEL);
    }
    else if (g_fallcheck_shared.state == FALLCHECK_STATE_NORMAL)
    {
        rt_pin_write(LED_PIN_B, LED_ON_LEVEL);
    }
    else
    {
        rt_pin_write(LED_PIN_G, LED_ON_LEVEL);
    }
}

static void led_task(void *parameter)
{
    (void)parameter;

    while (1)
    {
        /* 不做跨页面越权报警：当前页面是什么，LED 就显示什么任务。 */
        if (g_thermofall_page == THERMOFALL_PAGE_FALL)
        {
            fall_led_update();
        }
        else
        {
            thermo_led_update();
        }

        rt_thread_mdelay(LED_REFRESH_MS);
    }
}

static void thermometer_sample_once(void)
{
    float temperature;
    float humidity;
    rt_uint8_t alarm;

    if (g_aht10_dev == RT_NULL)
    {
        return;
    }

    humidity = aht10_read_humidity(g_aht10_dev);
    temperature = aht10_read_temperature(g_aht10_dev);
    alarm = (thermo_get_comfort_level(temperature, humidity) == THERMO_COMFORT_ALARM) ? 1 : 0;

    g_thermometer_shared.temperature = temperature;
    g_thermometer_shared.humidity = humidity;
    g_thermometer_shared.alarm = alarm;
    g_thermometer_shared.sensor_ok = 1;
    g_thermometer_shared.sequence++;

    rt_kprintf("[ThermoFall] thermo temp=%d.%d C, humi=%d.%d %%, alarm=%d, seq=%u\r\n",
               (int)temperature, (int)(app_absf(temperature) * 10) % 10,
               (int)humidity, (int)(app_absf(humidity) * 10) % 10,
               alarm, g_thermometer_shared.sequence);
}

static void thermometer_task(void *parameter)
{
    (void)parameter;

    rt_thread_mdelay(2000);

    g_aht10_dev = aht10_init(PKG_AHT10_I2C_BUS_NAME);
    if (g_aht10_dev == RT_NULL)
    {
        g_thermometer_shared.sensor_ok = 0;
        rt_kprintf("[ThermoFall] AHT20 init failed on %s\r\n", PKG_AHT10_I2C_BUS_NAME);
    }
    else
    {
        g_thermometer_shared.sensor_ok = 1;
        rt_kprintf("[ThermoFall] AHT20 ready on %s\r\n", PKG_AHT10_I2C_BUS_NAME);
    }

    while (1)
    {
        if (!g_thermometer_shared.paused)
        {
            thermometer_sample_once();
        }

        rt_thread_mdelay(THERMO_SAMPLE_PERIOD_MS);
    }
}

static int romfs_prepare(void)
{
    FILE *fp;
    char magic[4];

    if (dfs_mount(RT_NULL, "/", "rom", 0, (const void *)&romfs_root) != RT_EOK)
    {
        rt_kprintf("[ThermoFall] ROMFS mount skipped or failed\r\n");
    }

    fp = fopen(FALLCHECK_BEEP_PATH, "rb");
    if (fp == RT_NULL)
    {
        rt_kprintf("[ThermoFall] cannot open %s\r\n", FALLCHECK_BEEP_PATH);
        return -RT_ERROR;
    }

    if (fread(magic, 1, sizeof(magic), fp) != sizeof(magic) ||
        magic[0] != 'R' || magic[1] != 'I' || magic[2] != 'F' || magic[3] != 'F')
    {
        fclose(fp);
        rt_kprintf("[ThermoFall] %s is not a WAV file\r\n", FALLCHECK_BEEP_PATH);
        return -RT_ERROR;
    }

    fclose(fp);
    rt_kprintf("[ThermoFall] ROMFS beep ready: %s\r\n", FALLCHECK_BEEP_PATH);
    return RT_EOK;
}

static void alarm_task(void *parameter)
{
    rt_tick_t last_play_tick = 0;
    rt_uint8_t was_fall = 0;
    rt_uint8_t romfs_ok;

    (void)parameter;

    romfs_ok = (romfs_prepare() == RT_EOK) ? 1 : 0;

    while (1)
    {
        rt_uint8_t page_is_fall = (g_thermofall_page == THERMOFALL_PAGE_FALL) ? 1 : 0;
        rt_uint8_t is_fall = (page_is_fall &&
                              g_fallcheck_shared.state == FALLCHECK_STATE_FALL &&
                              g_fallcheck_shared.alarm &&
                              !g_fallcheck_shared.muted) ? 1 : 0;
        rt_tick_t now = rt_tick_get();
        rt_tick_t cooldown = rt_tick_from_millisecond(ALARM_BEEP_COOLDOWN_MS);

        /* 按用户要求：Thermo 页面不越权触发 Fall 声音报警。 */
        if (romfs_ok && is_fall && (!was_fall || (now - last_play_tick) >= cooldown))
        {
            last_play_tick = now;
            if (wavplayer_play(FALLCHECK_BEEP_PATH) != 0)
            {
                rt_kprintf("[ThermoFall] wavplayer failed: %s\r\n", FALLCHECK_BEEP_PATH);
            }
        }

        was_fall = is_fall;
        rt_thread_mdelay(ALARM_POLL_MS);
    }
}

static int start_thread(const char *name,
                        void (*entry)(void *parameter),
                        rt_uint32_t stack_size,
                        rt_uint8_t priority)
{
    rt_thread_t tid;

    tid = rt_thread_create(name, entry, RT_NULL, stack_size, priority, 20);
    if (tid == RT_NULL)
    {
        rt_kprintf("[ThermoFall] %s thread create failed\r\n", name);
        return -RT_ERROR;
    }

    rt_thread_startup(tid);
    return RT_EOK;
}

int main(void)
{
    rt_kprintf("ThermoFall: AHT20 + LSM6DS3 + LVGL switch + key/LED/audio\r\n");
    thermometer_shared_reset();

    rt_pin_mode(LED_PIN_R, PIN_MODE_OUTPUT);
    rt_pin_mode(LED_PIN_G, PIN_MODE_OUTPUT);
    rt_pin_mode(LED_PIN_B, PIN_MODE_OUTPUT);
    led_all_off();

    rt_pin_mode(BUTTON_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(BUTTON_PIN, PIN_IRQ_MODE_FALLING, key_callback, RT_NULL);
    rt_pin_irq_enable(BUTTON_PIN, PIN_IRQ_ENABLE);

    lvgl_thread_init();
    start_thread("tf_thermo", thermometer_task, 4096, 22);
    start_thread("tf_led", led_task, 1024, 23);
    start_thread("tf_alarm", alarm_task, 4096, 21);

    while (1)
    {
        rt_thread_mdelay(1000);
    }
}
