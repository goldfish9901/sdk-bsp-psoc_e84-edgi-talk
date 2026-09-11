/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-01-01     RT-Thread    First version
 */

#ifndef __XIAOZHI_UI_H__
#define __XIAOZHI_UI_H__

#include <rtthread.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * UI 初始化与同步
 */

/**
 * @brief 初始化 UI 子系统
 *
 * 该函数创建 UI 线程并初始化消息队列。
 * 调用后可使用 wait_ui_ready() 等待 UI 完成初始化。
 */
void xiaozhi_ui_init(void);

/**
 * @brief 等待 UI 初始化完成
 * @param timeout 超时时间，单位为 OS tick；RT_WAITING_FOREVER 表示一直等待
 * @return 成功返回 RT_EOK，超时返回 -RT_ETIMEOUT
 */
rt_err_t xiaozhi_ui_wait_ready(rt_int32_t timeout);

/**
 * @brief 查询 UI 消息队列是否已就绪（非阻塞）
 *
 * 与 xiaozhi_ui_wait_ready() 的区别：本函数只读取内部标志位，
 * 不消耗 s_ui_init_sem，可被多个线程反复调用。
 *
 * 典型用法：在 UI 子系统之外的线程里轮询本函数，确认消息队列初始化
 * 完成后再调用 xiaozhi_ui_set_xxx()，避免向未初始化的消息队列发送消息。
 *
 * @return 消息队列已就绪返回 RT_TRUE，否则返回 RT_FALSE
 */
rt_bool_t xiaozhi_ui_is_ready(void);

/*
 * UI 更新接口
 */

/**
 * @brief 更新聊天状态标签
 * @param status 需要显示的状态字符串
 */
void xiaozhi_ui_set_status(const char *status);

/**
 * @brief 更新聊天输出标签
 * @param output 需要显示的输出字符串
 */
void xiaozhi_ui_set_output(const char *output);

/**
 * @brief 更新表情显示
 * @param emoji 表情名称，例如 "happy"、"sad"、"neutral"
 */
void xiaozhi_ui_set_emoji(const char *emoji);

/**
 * @brief 更新 ADC 显示标签
 * @param adc_str 需要显示的 ADC 字符串
 */
void xiaozhi_ui_set_adc(const char *adc_str);

/**
 * @brief 更新 AHT20 温湿度显示标签
 * @param temp_humi_str 短文本，例如 "T 25.3C  H 55.0%"
 */
void xiaozhi_ui_set_temp_humi(const char *temp_humi_str);

/**
 * @brief 清空信息标签 label2
 */
void xiaozhi_ui_clear_info(void);

/**
 * @brief 在屏幕上显示 AP 配网信息
 * @param ap_info AP 信息文本，例如 "SSID: xxx 密码: xxx IP:192.168.169.1"
 */
void xiaozhi_ui_show_ap_config(const char *ap_info);

/**
 * @brief 显示连接中状态，用于已保存配置的自动连接过程
 */
void xiaozhi_ui_show_connecting(void);

/**
 * @brief 更新电量显示
 * @param level 电量百分比，范围 0-100
 */
void xiaozhi_ui_update_battery(int level);

/**
 * @brief 更新充电状态显示
 * @param is_charging 正在充电为 true，否则为 false
 */
void xiaozhi_ui_update_charging_status(bool is_charging);

/**
 * @brief 更新 BLE 连接状态图标
 * @param connected BLE 已连接为 true，否则为 false
 * @note 当前未实现蓝牙显示功能
 */
void xiaozhi_ui_update_ble_status(bool connected);

/* 旧接口兼容：保留给原 XiaoZhi 代码调用 */

/**
 * @brief 旧接口：初始化 UI 子系统
 * @deprecated 建议改用 xiaozhi_ui_init()
 */
void init_ui(void);

/**
 * @brief 旧接口：等待 UI 初始化完成
 * @deprecated 建议改用 xiaozhi_ui_wait_ready()
 */
rt_err_t wait_ui_ready(rt_int32_t timeout);

/**
 * @brief 旧接口：清空信息标签
 * @deprecated 建议改用 xiaozhi_ui_clear_info()
 */
void clean_info(void);

/**
 * @brief 旧接口：更新聊天状态
 * @deprecated 建议改用 xiaozhi_ui_set_status()
 */
void xiaozhi_ui_chat_status(char *string);

/**
 * @brief 旧接口：更新聊天输出
 * @deprecated 建议改用 xiaozhi_ui_set_output()
 */
void xiaozhi_ui_chat_output(char *string);

/**
 * @brief 旧接口：更新表情显示
 * @deprecated 建议改用 xiaozhi_ui_set_emoji()
 */
void xiaozhi_ui_update_emoji(char *string);

/**
 * @brief 旧接口：更新 ADC 显示
 * @deprecated 建议改用 xiaozhi_ui_set_adc()
 */
void xiaozhi_ui_update_adc(char *string);

#ifdef __cplusplus
}
#endif

#endif /* __XIAOZHI_UI_H__ */