#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <lv_rt_thread_conf.h>
#include "lv_port_disp.h"
#include "aht10.h"
#include "exam_shared.h"

#define LED_PIN_B                 GET_PIN(16, 5)
#define LED_PIN_G                 GET_PIN(16, 6)
#define LED_PIN_R                 GET_PIN(16, 7)
#define LED_ON_LEVEL              PIN_HIGH
#define LED_OFF_LEVEL             PIN_LOW
#define BUTTON_PIN                GET_PIN(8, 3)
#define SAMPLE_PERIOD_MS          1000
#define LED_REFRESH_MS            10

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

static aht10_device_t g_aht10_dev = RT_NULL;

void ui_thermometer_init(void);

static void thermometer_shared_reset(void)
{
    /* 单烧录工程不再依赖跨核共享区，启动时明确初始化 UI 数据缓存。 */
    g_thermometer_shared.magic = THERMOMETER_SHARED_MAGIC;
    g_thermometer_shared.sequence = 0;
    g_thermometer_shared.temperature = 0.0f;
    g_thermometer_shared.humidity = 0.0f;
    g_thermometer_shared.paused = 0;
    g_thermometer_shared.alarm = 0;
    g_thermometer_shared.sensor_ok = 0;
    g_thermometer_shared.reserved = 0;
}

void lv_user_gui_init(void)
{
    ui_thermometer_init();
}
#define LCD_BL_GPIO_NUM    GET_PIN(15, 7)   /* LCD 背光电源开关 */
#define BL_PWM_DISP_CTRL   GET_PIN(20, 6)   /* LCD PWM 亮度调节 */

static void m55_lcd_backlight_enable(void)
{
    rt_pin_mode(LCD_BL_GPIO_NUM, PIN_MODE_OUTPUT);
    rt_pin_mode(BL_PWM_DISP_CTRL, PIN_MODE_OUTPUT);
    rt_pin_write(LCD_BL_GPIO_NUM, PIN_HIGH);
    rt_pin_write(BL_PWM_DISP_CTRL, PIN_HIGH);
}

static float thermo_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static void thermometer_key_callback(void *args)
{
    (void)args;

    /* 中断里只翻转暂停标志，主循环负责所有较慢的传感器读数工作。 */
    g_thermometer_shared.paused = !g_thermometer_shared.paused;
}

static void thermometer_led_all_off(void)
{
    rt_pin_write(LED_PIN_R, LED_OFF_LEVEL);
    rt_pin_write(LED_PIN_G, LED_OFF_LEVEL);
    rt_pin_write(LED_PIN_B, LED_OFF_LEVEL);
}

static void thermometer_led_update(float temperature, float humidity)
{
    rt_tick_t now;
    rt_uint8_t blink_on;
    thermo_comfort_level_t level;

    /* 板载三色灯按 HIGH 点亮、LOW 熄灭处理。
     * 每 10ms 重写一次三灯状态，压住 M33 残留程序的蓝灯心跳。 */
    now = rt_tick_get();
    blink_on = ((now / rt_tick_from_millisecond(1000)) % 2) ? 1 : 0;
    level = thermo_get_comfort_level(temperature, humidity);

    thermometer_led_all_off();

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

    rt_kprintf("[Single Thermo] temp=%d.%d C, humi=%d.%d %%, alarm=%d, seq=%u\r\n",
               (int)temperature, (int)(thermo_absf(temperature) * 10) % 10,
               (int)humidity, (int)(thermo_absf(humidity) * 10) % 10,
               alarm, g_thermometer_shared.sequence);
}

int main(void)
{
    rt_kprintf("Single Thermometer M55: AHT20 + LVGL + key/LED\r\n");
    thermometer_shared_reset();

    rt_pin_mode(LED_PIN_R, PIN_MODE_OUTPUT);
    rt_pin_mode(LED_PIN_G, PIN_MODE_OUTPUT);
    rt_pin_mode(LED_PIN_B, PIN_MODE_OUTPUT);
    thermometer_led_all_off();
    rt_pin_mode(BUTTON_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(BUTTON_PIN, PIN_IRQ_MODE_FALLING, thermometer_key_callback, RT_NULL);
    rt_pin_irq_enable(BUTTON_PIN, PIN_IRQ_ENABLE);
    lvgl_thread_init();
    rt_thread_mdelay(500);           /* 给 LVGL 首帧留点时间 */
    m55_lcd_backlight_enable();      /* 点亮背光 */
    rt_thread_mdelay(1500);


    g_aht10_dev = aht10_init(PKG_AHT10_I2C_BUS_NAME);
    if (g_aht10_dev == RT_NULL)
    {
        g_thermometer_shared.sensor_ok = 0;
        rt_kprintf("[Single Thermo] AHT20 init failed on %s\r\n", PKG_AHT10_I2C_BUS_NAME);
    }
    else
    {
        g_thermometer_shared.sensor_ok = 1;
        rt_kprintf("[Single Thermo] AHT20 ready on %s\r\n", PKG_AHT10_I2C_BUS_NAME);
    }

    while (1)
    {
        static rt_uint32_t sample_elapsed = SAMPLE_PERIOD_MS;

        if (!g_thermometer_shared.paused && sample_elapsed >= SAMPLE_PERIOD_MS)
        {
            thermometer_sample_once();
            sample_elapsed = 0;
        }

        thermometer_led_update(g_thermometer_shared.temperature, g_thermometer_shared.humidity);
        rt_thread_mdelay(LED_REFRESH_MS);
        sample_elapsed += LED_REFRESH_MS;
    }
}

