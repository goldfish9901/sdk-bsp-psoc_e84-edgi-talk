# Edgi_Talk_M55_ThermoFall

《PSoC开发实践指南》**实验 4** 的工程实现，即指南中的 ThermoFall。

温湿度与摔倒检测融合工程。一个 M55 工程内同时运行 AHT20 温湿度任务和 LSM6DS3 摔倒检测任务，LVGL 通过触摸按钮切换 `Thermo` / `FallCheck` 页面。

## 构建

需先在 RT-Thread ENV 里（见 [`../../exp-docs/RT-Thread-ENV使用说明.md`](../../exp-docs/RT-Thread-ENV使用说明.md)）：

```bat
mklinks.bat
scons -j12
```

产出工程根目录下的 `rtthread.elf` / `rtthread.hex`。详细步骤、配置约定与上板验收清单见
[`docs/工程构建说明.md`](docs/工程构建说明.md)。

## 功能

- Thermo 页面：显示 AHT20 温度、湿度、舒适度和采样计数。
- FallCheck 页面：显示 LSM6DS3 ACC、GYR、`IDLE/MOVING/FALL` 状态。
- 触摸按钮切换页面。
- KEY 跟随当前页面执行暂停或静音。
- LED 跟随当前页面任务显示。
- FallCheck 页面确认 FALL 且未静音时播放 `/webnet/beep.wav`。
- 编译输出统一为 `Debug/rtthread.elf`。

## LED 逻辑

Thermo 页面：

| 状态 | LED |
| --- | --- |
| COMFORT | 绿灯 2 秒节奏闪烁 |
| WARM | 红灯 2 秒节奏闪烁 |
| ALARM | 红灯常亮 |
| DRY/COOL | 蓝灯 2 秒节奏闪烁 |

FallCheck 页面：

| 状态 | LED |
| --- | --- |
| IDLE | 绿灯常亮 |
| NORMAL/MOVING | 蓝灯常亮 |
| FALL | 红灯常亮 |

后台 Fall 检测到 FALL 时，如果当前仍在 Thermo 页面，LED 和声音不切到 Fall 报警。

## KEY / Touch

- 触摸按钮：切换 Thermo / FallCheck 页面。
- Thermo 页面下 KEY：暂停/恢复温湿度采样。
- FallCheck 页面下 KEY：切换报警静音。

## 主要文件

- `applications/main.c`：任务启动、KEY、LED 仲裁、报警音逻辑。
- `applications/ui.c`：双页面 LVGL 界面。
- `applications/exam_shared.h`：温湿度、摔倒检测、当前页面共享结构。
- `packages/lsm6ds3tr/lsm6ds3tr-c_port.c`：LSM6DS3 采样和摔倒判断。
- `packages/aht10-latest`：AHT20 采样驱动。
