#include "xiaozhi_ui.h"
#include <rtdevice.h>
#include <rtthread.h>


#define XZ_ENV_SAMPLE_PERIOD_MS 2000
/* i2c1(105/106) 已让给 ST7102 触摸面板，AHT20 独占 i2c2(75/74) */
#define XZ_AHT20_I2C_BUS_NAME "i2c2"
#define XZ_AHT20_ADDR 0x38
#define XZ_AHT20_STATUS_BUSY 0x80
#define XZ_AHT20_STATUS_CALIBRATED 0x08
#define XZ_AHT20_CMD_INIT 0xBE
#define XZ_AHT20_CMD_TRIGGER 0xAC
#define XZ_AHT20_CMD_SOFT_RESET 0xBA

static rt_err_t xz_aht20_send3(struct rt_i2c_bus_device *bus, rt_uint8_t cmd,
                               rt_uint8_t arg0, rt_uint8_t arg1) {
  rt_uint8_t packet[3];

  packet[0] = cmd;
  packet[1] = arg0;
  packet[2] = arg1;

  return (rt_i2c_master_send(bus, XZ_AHT20_ADDR, 0, packet, sizeof(packet)) ==
          (rt_ssize_t)sizeof(packet))
             ? RT_EOK
             : -RT_ERROR;
}

static rt_err_t xz_aht20_send1(struct rt_i2c_bus_device *bus, rt_uint8_t cmd) {
  return (rt_i2c_master_send(bus, XZ_AHT20_ADDR, 0, &cmd, 1) == 1) ? RT_EOK
                                                                   : -RT_ERROR;
}

static rt_err_t xz_aht20_read(struct rt_i2c_bus_device *bus, rt_uint8_t *data,
                              rt_uint32_t len) {
  return (rt_i2c_master_recv(bus, XZ_AHT20_ADDR, 0, data, len) ==
          (rt_ssize_t)len)
             ? RT_EOK
             : -RT_ERROR;
}

static rt_bool_t xz_aht20_decode(const rt_uint8_t data[6], int *temp_x10,
                                 int *humi_pct) {
  rt_uint32_t humidity_raw;
  rt_uint32_t temp_raw;
  rt_int32_t temp_calc_x10;
  rt_uint32_t humidity_calc;

  if ((data[0] & XZ_AHT20_STATUS_BUSY) != 0) {
    return RT_FALSE;
  }

  humidity_raw = ((rt_uint32_t)data[1] << 12) | ((rt_uint32_t)data[2] << 4) |
                 (((rt_uint32_t)data[3] & 0xF0) >> 4);
  temp_raw = (((rt_uint32_t)data[3] & 0x0F) << 16) |
             ((rt_uint32_t)data[4] << 8) | (rt_uint32_t)data[5];

  temp_calc_x10 = (rt_int32_t)((temp_raw * 2000U + 524288U) / 1048576U) - 500;
  humidity_calc = (humidity_raw * 100U + 524288U) / 1048576U;
  if (humidity_calc > 100U) {
    humidity_calc = 100U;
  }

  *temp_x10 = (int)temp_calc_x10;
  *humi_pct = (int)humidity_calc;
  return RT_TRUE;
}

static rt_bool_t xz_aht20_read_once(struct rt_i2c_bus_device *bus,
                                    int *temp_x10, int *humi_pct) {
  rt_uint8_t status = 0;
  rt_uint8_t data[6] = {0};

  if (xz_aht20_send3(bus, XZ_AHT20_CMD_INIT, 0x08, 0x00) != RT_EOK) {
    xz_aht20_send1(bus, XZ_AHT20_CMD_SOFT_RESET);
    rt_thread_mdelay(20);
    if (xz_aht20_send3(bus, XZ_AHT20_CMD_INIT, 0x08, 0x00) != RT_EOK) {
      return RT_FALSE;
    }
  }
  rt_thread_mdelay(10);

  if (xz_aht20_read(bus, &status, 1) != RT_EOK) {
    return RT_FALSE;
  }

  if ((status & XZ_AHT20_STATUS_CALIBRATED) == 0) {
    return RT_FALSE;
  }

  if (xz_aht20_send3(bus, XZ_AHT20_CMD_TRIGGER, 0x33, 0x00) != RT_EOK) {
    return RT_FALSE;
  }
  rt_thread_mdelay(80);

  if (xz_aht20_read(bus, data, sizeof(data)) != RT_EOK) {
    return RT_FALSE;
  }

  return xz_aht20_decode(data, temp_x10, humi_pct);
}

static void xz_temp_humi_thread(void *parameter) {
  struct rt_i2c_bus_device *bus;
  char text[64];

  (void)parameter;

  /* 等待 XiaoZhi UI 子系统建好，替代参考实现里的固定 rt_thread_mdelay(2500)。
   * xiaozhi_ui_is_ready() 只读一个 bool，不触碰任何 IPC 对象，可安全轮询；
   * 每轮 100ms 同时给软件 I2C 驱动留出注册时间。 */
  while (xiaozhi_ui_is_ready() != RT_TRUE) {
    rt_thread_mdelay(100);
  }

  /* 再等首帧就绪（此时 s_ui_init_sem 已完成 init）。
   * 注意：s_ui_init_sem 只在 UI 线程启动时 release 一次且不可重置，
   * 因此全工程只在这里调用一次；其他位置请改用 xiaozhi_ui_is_ready()。 */
  xiaozhi_ui_wait_ready(rt_tick_from_millisecond(5000));

  bus = rt_i2c_bus_device_find(XZ_AHT20_I2C_BUS_NAME);
  if (bus == RT_NULL) {
    /* 文案与总线名绑定，避免改名后 UI 提示与实际总线不一致 */
    rt_snprintf(text, sizeof(text), "AHT20: %s missing", XZ_AHT20_I2C_BUS_NAME);
    xiaozhi_ui_set_temp_humi(text);
    rt_kprintf("[XiaoZhi Env] AHT20 I2C bus %s not found\r\n",
               XZ_AHT20_I2C_BUS_NAME);
    return;
  }

  rt_kprintf("[XiaoZhi Env] AHT20 ready on %s\r\n", XZ_AHT20_I2C_BUS_NAME);

  while (1) {
    int temp_x10 = 0;
    int humi_pct = 0;
    int temp_abs_x10;

    if (xz_aht20_read_once(bus, &temp_x10, &humi_pct)) {
      temp_abs_x10 = temp_x10 < 0 ? -temp_x10 : temp_x10;
      rt_snprintf(text, sizeof(text), "T %s%d.%dC  H %d%%",
                  temp_x10 < 0 ? "-" : "", temp_abs_x10 / 10, temp_abs_x10 % 10,
                  humi_pct);
      xiaozhi_ui_set_temp_humi(text);
      rt_kprintf("[XiaoZhi Env] %s\r\n", text);
    } else {
      xiaozhi_ui_set_temp_humi("AHT20: retry");
      rt_kprintf("[XiaoZhi Env] AHT20 read failed, retry\r\n");
    }

    rt_thread_mdelay(XZ_ENV_SAMPLE_PERIOD_MS);
  }
}

int xz_temp_humi_init(void) {
  rt_thread_t tid;

  tid = rt_thread_create("xz_env", xz_temp_humi_thread, RT_NULL, 2048, 24, 20);
  if (tid == RT_NULL) {
    rt_kprintf("[XiaoZhi Env] create thread failed\r\n");
    return -RT_ERROR;
  }

  rt_thread_startup(tid);
  return RT_EOK;
}
INIT_APP_EXPORT(xz_temp_humi_init);