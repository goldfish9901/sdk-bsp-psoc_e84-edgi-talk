/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <board.h>
#include <rtdevice.h>

#ifdef BSP_XIAOZHI_USB_CONSOLE
#if defined(RT_CHERRYUSB_DEVICE_SPEED_HS) && !defined(CONFIG_USB_HS)
#define CONFIG_USB_HS
#endif
#include "usbd_core.h"
#include "usbd_cdc_acm.h"

/*!< endpoint address */
#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< config descriptor size */
#define USB_CONFIG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN)

#ifdef CONFIG_USB_HS
#define CDC_MAX_MPS 512
#else
#define CDC_MAX_MPS 64
#endif

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02)
};

static const uint8_t fs_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, 64, 0x02)
};

static const uint8_t other_hs_descriptor[] = {
    0x09, USB_DESCRIPTOR_TYPE_OTHER_SPEED, WBVAL(USB_CONFIG_SIZE),
    0x02, 0x01, 0x00, USB_CONFIG_BUS_POWERED, USB_CONFIG_POWER_MA(USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, 512, 0x02)
};
static const uint8_t other_fs_descriptor[] = {
    0x09, USB_DESCRIPTOR_TYPE_OTHER_SPEED, WBVAL(USB_CONFIG_SIZE),
    0x02, 0x01, 0x00, USB_CONFIG_BUS_POWERED, USB_CONFIG_POWER_MA(USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, 64, 0x02)
};

static const uint8_t device_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, 0x01)
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "CherryUSB",                  /* Manufacturer */
    "Edgi Talk USB Console",         /* Product */
    "2022123456",                 /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    return speed == USB_SPEED_HIGH ? config_descriptor : fs_config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    return device_quality_descriptor;
}

static const uint8_t *other_speed_descriptor_callback(uint8_t speed)
{
    return speed == USB_SPEED_HIGH ? other_fs_descriptor : other_hs_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index > 3) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor cdc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .other_speed_descriptor_callback = other_speed_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback
};

/* Queue logs before enumeration and keep logging independent of USB/host speed.
 * If the host cannot keep up, new bytes beyond the queue capacity are dropped. */
#define CONSOLE_TX_SIZE 8192
static struct rt_device console_device;
static struct rt_ringbuffer tx_ring, rx_ring;
static rt_uint8_t tx_storage[CONSOLE_TX_SIZE], rx_storage[2048];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t rx_packet[CDC_MAX_MPS];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t tx_packet[CDC_MAX_MPS];
static volatile bool configured, terminal_open, tx_busy;
static struct usbd_interface control_intf, data_intf;

static rt_ssize_t console_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_base_t level = rt_hw_interrupt_disable();
    rt_size_t count = rt_ringbuffer_get(&rx_ring, buffer, size);
    rt_hw_interrupt_enable(level);
    return count;
}

static rt_ssize_t console_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    const rt_uint8_t *bytes = buffer;
    rt_base_t level = rt_hw_interrupt_disable();
    rt_size_t count;
    for (count = 0; count < size; count++)
    {
        if ((dev->open_flag & RT_DEVICE_FLAG_STREAM) && bytes[count] == '\n'
            && (count == 0 || bytes[count - 1] != '\r'))
        {
            if (rt_ringbuffer_space_len(&tx_ring) < 2) break;
            rt_ringbuffer_putchar(&tx_ring, '\r');
        }
        if (!rt_ringbuffer_putchar(&tx_ring, bytes[count])) break;
    }
    rt_hw_interrupt_enable(level);
    return count;
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops console_ops = {
    .read = console_read,
    .write = console_write,
};
#endif

static int usb_console_register(void)
{
    rt_ringbuffer_init(&tx_ring, tx_storage, sizeof(tx_storage));
    rt_ringbuffer_init(&rx_ring, rx_storage, sizeof(rx_storage));
    console_device.type = RT_Device_Class_Char;
#ifdef RT_USING_DEVICE_OPS
    console_device.ops = &console_ops;
#else
    console_device.read = console_read;
    console_device.write = console_write;
#endif
    if (rt_device_register(&console_device, "usb-acm0",
                          RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX) != RT_EOK)
        return -RT_ERROR;
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
    return RT_EOK;
}
INIT_BOARD_EXPORT(usb_console_register);

static void console_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    rt_ringbuffer_put(&rx_ring, rx_packet, nbytes);
    usbd_ep_start_read(busid, ep, rx_packet, sizeof(rx_packet));
    if (console_device.rx_indicate)
        console_device.rx_indicate(&console_device, rt_ringbuffer_data_len(&rx_ring));
}

static void console_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if (nbytes && nbytes % usbd_get_ep_mps(busid, ep) == 0)
    {
        if (usbd_ep_start_write(busid, ep, NULL, 0) < 0) tx_busy = false;
    }
    else tx_busy = false;
}

static struct usbd_endpoint console_out_ep = { .ep_addr = CDC_OUT_EP, .ep_cb = console_out };
static struct usbd_endpoint console_in_ep = { .ep_addr = CDC_IN_EP, .ep_cb = console_in };

static void console_event(uint8_t busid, uint8_t event)
{
    if (event == USBD_EVENT_CONFIGURED)
    {
        configured = true;
        tx_busy = false;
        usbd_ep_start_read(busid, CDC_OUT_EP, rx_packet, sizeof(rx_packet));
    }
    else if (event == USBD_EVENT_RESET || event == USBD_EVENT_DISCONNECTED)
    {
        configured = terminal_open = tx_busy = false;
    }
}

void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
{
    terminal_open = dtr;
}

static void console_tx_entry(void *parameter)
{
    while (1)
    {
        rt_base_t level = rt_hw_interrupt_disable();
        if (configured && terminal_open && !tx_busy)
        {
            rt_size_t count = rt_ringbuffer_get(&tx_ring, tx_packet,
                                                usbd_get_ep_mps(0, CDC_IN_EP));
            if (count)
            {
                tx_busy = true;
                if (usbd_ep_start_write(0, CDC_IN_EP, tx_packet, count) < 0)
                    tx_busy = false;
            }
        }
        rt_hw_interrupt_enable(level);
        rt_thread_mdelay(1);
    }
}

static int usb_console_start(void)
{
    rt_thread_t thread;
    int result;
    usbd_desc_register(0, &cdc_descriptor);
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &control_intf));
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &data_intf));
    usbd_add_endpoint(0, &console_out_ep);
    usbd_add_endpoint(0, &console_in_ep);
    result = usbd_initialize(0, USBHS_BASE, console_event);
    if (result < 0) return result;
    thread = rt_thread_create("usb_log", console_tx_entry, RT_NULL, 2048, 20, 10);
    if (!thread) return -RT_ENOMEM;
    return rt_thread_startup(thread);
}
INIT_ENV_EXPORT(usb_console_start);
#endif
