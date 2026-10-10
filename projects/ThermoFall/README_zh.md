# ThermoFall 单核烧录融合工程

## 工程定位

`ThermoFall` 是一个只需要烧录一次的 M55 工程，把温湿度显示和人体摔倒检测融合在同一个 RT-Thread Studio 工程里。

工程包含：

- AHT20 温湿度采集和舒适度判断
- LSM6DS3 加速度 / 陀螺仪采集和摔倒检测
- LVGL 触摸屏界面
- 触控按钮切换 Thermo / FallCheck 页面
- 板载 KEY 中断
- 三色 LED 状态提示
- ROMFS 内置 `beep.wav`，Fall 报警时播放声音

## 页面和按键

- 触摸屏按钮：切换 `Thermo` 和 `FallCheck` 页面。
- 当前在 `Thermo` 页面时，板载 KEY 用来暂停 / 恢复温湿度采样。
- 当前在 `FallCheck` 页面时，板载 KEY 用来切换报警静音。

## LED 规则

### Thermo 页面

Thermo 页面使用温湿度舒适度逻辑：

- Comfort：绿灯 2 秒节奏闪烁
- Warm：红灯 2 秒节奏闪烁
- Alarm：红灯常亮
- Dry / Cool：蓝灯 2 秒节奏闪烁

### FallCheck 页面

FallCheck 页面使用摔倒检测状态逻辑：

- IDLE：绿灯常亮
- MOVING / NORMAL：蓝灯常亮
- FALL：红灯常亮

注意：FallCheck 的 FALL 状态不会越权覆盖 Thermo 页面。当前显示哪个页面，LED 就跟随哪个页面的任务状态。静音只影响声音播放，不影响 FALL 红灯显示。

## 关键配置

- 工程名：`ThermoFall`
- 生成 ELF：`Debug/rtthread.elf`
- OpenOCD 下载目标：`Debug/rtthread.elf`
- AHT20 I2C：`i2c2`
- AHT20 软件 I2C 引脚：SCL `GET_PIN(9, 3)` / SDA `GET_PIN(9, 2)`，对应宏值 `75` / `74`
- 报警音频路径：`/webnet/beep.wav`

## 现场小改建议

- 修改温湿度阈值：看 `applications/exam_shared.h` 里的 `thermo_get_comfort_level()`。
- 修改摔倒判断：看 `applications/fallcheck_core.c`。
- 修改页面文字和颜色：看 `applications/ui.c`。
- 修改 LED 行为：看 `applications/main.c` 里的 `thermo_led_update()` 和 `fall_led_update()`。
