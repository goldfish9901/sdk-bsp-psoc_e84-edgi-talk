# 本地实验工程与 Env 构建

上游 SDK 文档：<https://rt-thread-studio.github.io/sdk-bsp-psoc_e84-edgi-talk/>。
本文只说明本分支增加或修改的部分；上游自带的库、工具、固件、图片和文档保持原样。
本地对照基线为 `b135be8a28127e276a04b70d27950c3ba07b917a`。

## 工程改动

| 工程 | 本分支的实际变化 |
| --- | --- |
| [M33 Driver All](Edgi_Talk_M33_Driver_All/README_zh.md) | 打开 `SOC_Enable_CM55`；SCons 增加编译数据库生成 |
| [M55 LVGL](Edgi_Talk_M55_LVGL/README_zh.md) | 绿灯翻转间隔改为 100 ms，Virtual3D 标题颜色改为 `0xaaffff`；保留上游 Virtual3D 默认示例 |
| [M55 WIFI](Edgi_Talk_M55_WIFI/README_zh.md) | 当前配置切换为 `WHD_RESOURCES_IN_MEMORY`，Wi-Fi 资源编入应用 |
| [M55 XiaoZhi](Edgi_Talk_M55_XiaoZhi/README_zh.md) | 新增 AHT20 温湿度显示；软件 `i2c2` 独占传感器，`i2c1` 保留给触摸屏 |
| [M55 ThermoFall](Edgi_Talk_M55_ThermoFall/README.md) | 新增温湿度与摔倒检测融合工程、双页面 UI、按键和声音报警 |
| [实验资料](exp-docs/README.md) | 课程资料、实验指南和本地 Env 使用说明 |

共享驱动的本地改动是 `libraries/HAL_Drivers/drv_soft_i2c.*`、
`libraries/HAL_Drivers/SConscript` 和 `libraries/M55_Config/Kconfig` 中的软件
I2C2 支持。这些文本与工程配置一起保留，不能只复制应用目录到未经修改的上游 SDK。

## 用 Env 复现构建产物

安装 [RT-Thread Env](https://www.rt-thread.org/download.html#download-rt-thread-env-tool)，
在 Windows CMD 中初始化环境，路径替换为自己的安装位置：

```bat
call E:\sdk\rtthread\env-windows-v2.0.0\env-windows\tools\bin\env-init.bat
cd /d E:\workspace-main\sdk-bsp-psoc_e84-edgi-talk\projects\Edgi_Talk_M55_ThermoFall
mklinks.bat
scons -j12
```

其他工程换到对应目录执行。`mklinks.bat` 仅在首次建立联接时运行；已有正确联接无需重复创建。
`RTT_EXEC_PATH` 指向 Env 的 `arm-none-eabi-gcc` 所在目录，并覆盖 `rtconfig.py`
中的 Studio 默认路径。可用 `where arm-none-eabi-gcc` 和 `arm-none-eabi-gcc --version`
核对实际工具链。本机 Env 提供 GCC 10.3.1；不同编译器构建不保证产物逐字节一致。

| 本地产物 | 复现方式 |
| --- | --- |
| `build/`、`.o`、ELF、HEX、MAP、`.sconsign.dblite` | 在对应工程执行 `scons -j12`；最终镜像路径见该工程 README |
| `Debug/` | Studio 的托管构建输出；Env/SCons 的输出路径可能不同，不需要为它增加拷贝步骤 |
| M33 Driver All 的 `compile_commands.json` | `scons compile_commands.json`（使用该工程已有的 `CompilationDatabase` 目标） |
| XiaoZhi 的 `scons_build.log` | CMD 中执行 `scons -j12 > scons_build.log 2>&1` |
| `cmake-build-*`、编辑器缓存 | 本地 IDE 按已保存的文本工程配置重新生成，不属于固件输入 |
| ThermoFall 的 `.settings/.rtmenus` | Studio 的 RT-Thread Settings 缓存；Env 构建不需要，不用于恢复配置 |

源码、`SConstruct`、`SConscript`、`rtconfig.h`、`.config`、Kconfig、链接脚本、
CMake/IDE 文本配置和 README 保留在 Git。已有 `packages/` 包含本地移植代码；
不要用 `pkgs --update` 批量覆盖，尤其 ThermoFall 的传感器和音频适配。
这份工作区维护 `rtconfig.h` 后可用 `scons --genconfig` 同步 `.config`；
该命令不是固件构建。配置工具的限制见 [Env 使用说明](exp-docs/RT-Thread-ENV使用说明.md)。

## AHT10 子模块

LVGL 和 XiaoZhi 的 `packages/aht10-latest` 固定到
`685d955b5031a55627043277bffa8cf7795681f0`，源码来自
<https://github.com/RT-Thread-packages/aht10>。在 SDK 根目录可初始化 LVGL 的子模块：

```bat
git submodule update --init -- projects/Edgi_Talk_M55_LVGL/packages/aht10-latest
```

当前 `.gitmodules` 中 XiaoZhi 的路径多写了一层 `projects/`，直接递归更新会受影响。
不修改子模块配置时，在 **XiaoZhi 目录且目标目录尚不存在**的情况下可手动取得固定版本：

```bat
git clone https://github.com/RT-Thread-packages/aht10.git packages/aht10-latest
git -C packages/aht10-latest checkout 685d955b5031a55627043277bffa8cf7795681f0
```

已有内容不要覆盖。ThermoFall 的 AHT10 是随工程保存的源码副本，不是子模块。

## 构建后烧录

在已初始化的 Env 中安装脚本依赖，使用参数名传入 OpenOCD 目录：

```bat
python -m pip install pydantic pydantic-cli
python ..\start.py --openOcdInstallDir C:\path\to\OpenOCD --projectMainDir .
```

`start.py` 先执行 SCons，成功后才调用 OpenOCD。项目根目录可手动新建文本文件
`.flash-target`，只写一行相对项目根目录的最终 HEX 路径，例如 ThermoFall：

```text
rtthread.hex
```

M33 Driver All 的最终合并镜像应写 `build/rtthread.hex`，不能选
`build/rtthread_app.hex` 或 `build/secure_fw/rtthread.hex` 等中间镜像。
其他三个 M55 工程的 SCons HEX 为 `rtthread.hex`。
`.flash-target` 是构建输入约定，可提交 Git；此处不自动创建，以免改变现有烧录选择。

没有 `.flash-target` 时，脚本仍按原来的 `rglob("*.hex")` 顺序选择首个文件。
有该文件但内容无效时直接报错，不退回扫描；路径必须指向项目内实际存在的 HEX，
不能包含空白或 Tcl 特殊字符。`extraHexFileScanDir` 参数当前保留但未用于搜索。

## 新增资源的保存原则

只停止跟踪本分支新增的可再生成产物、资源副本和课程二进制；文件仍留在本地。
不移除上游已有资源，不重写历史，不忽略整个 `packages/` 或源码目录。
新增 ThermoFall 副本的获取方式见其 README；课程 DOCX、图片和主题文件见
[实验资料 README](exp-docs/README.md)。厂商库、Wi-Fi 固件与上游工具继续沿用 SDK 自带版本。
