# RT-Thread ENV 工具使用说明

本文件说明本工作区（`sdk-bsp-psoc_e84-edgi-talk`）里 RT-Thread ENV 工具的用法，
以及它和 `scons` / `menuconfig` / `pkgs` 的关系。适用于 `projects/` 下所有工程。

---

## 1. ENV 是什么

ENV 是 RT-Thread 官方的**命令行构建环境**，本质是一个预配置好的 shell，里面装好了：

| 组件 | 说明 |
| --- | --- |
| Python 3.11.9 | `tools\python-3.11.9-amd64\`，并在 `.venv\` 建好虚拟环境 |
| SCons | 构建工具，RT-Thread 用它代替 make/CMake |
| `menuconfig` / `guiconfig` | 基于 kconfiglib 的 Kconfig 配置界面 |
| `pkgs` | RT-Thread 软件包管理器 |
| `arm-none-eabi-gcc` | ENV 自带的 arm gcc 10.3.1 |
| 常用 Unix 工具 | `make`、`sh`、`rm`、`ls`、`cp` 等（`tools\bin\`） |

RT-Thread Studio IDE **不是必须的**。Studio 只是把这套 scons 流程包了一层 GUI，
本工作区完全可以用 ENV 命令行完成配置、编译、下载。

---

## 2. 进入 ENV

本机的 ENV 装在 `E:\sdk\rtthread\env-windows-v2.0.0\env-windows`。进入方式：

```bat
cmd /c "call E:\sdk\rtthread\env-windows-v2.0.0\env-windows\tools\bin\env-init.bat && pwsh.exe"
```

`env-init.bat` 做的事（见该文件）：

```bat
set ENV_ROOT=%~dp0..\..                                        :: ENV 根目录
set RTT_EXEC_PATH=%ENV_ROOT%\tools\gnu_gcc\arm_gcc\mingw\bin   :: ★ arm gcc 10.3.1
set RTT_CC=gcc
set PKGS_ROOT=%ENV_ROOT%\packages
:: 把 tools\git...\cmd、tools\bin、%RTT_EXEC_PATH%、qemu 加入 PATH
chcp 65001                                                     :: UTF-8 代码页
call %VENV%\Scripts\activate.bat                               :: 激活 Python 3.11.9 venv
```

它开头的 `if not "%ENV_ROOT%" == "" goto end` 保证**不重复初始化**；
`env.exe` / `env.bat` 会拉起 ConEmu，但 ConEmu 默认任务**不会**自动执行 `env-init.bat`，
需要手动 `call` 一次，或把调用写进 `%USERPROFILE%\.conemu\CmdInit.cmd`。

> ENV 的路径**不能包含中文或空格**（`env-init.bat` 里有检查并会告警）。

---

## 3. 工具链是怎么被找到的（重要）

`projects/<工程>/rtconfig.py` 里写死了一个默认工具链路径：

```python
EXEC_PATH = r'C:\RT-ThreadStudio\repo\Extract\ToolChain_Support_Packages\ARM\GNU_Tools_for_ARM_Embedded_Processors\13.3\bin'
```

**这条路径在本机并不存在**，但它无害，原因是 `rtconfig.py` 末尾有覆盖逻辑：

```python
if os.getenv('RTT_EXEC_PATH'):
    EXEC_PATH = os.getenv('RTT_EXEC_PATH')     # rtconfig.py:28-29
```

而 `env-init.bat` 正好设置了 `RTT_EXEC_PATH`。于是：

- 在 ENV shell 里构建 → `EXEC_PATH` 被覆盖成 ENV 的 gcc 10.3.1 → **正常编译**
- `rt-thread/tools/building.py:192` 只有在 `rtconfig.EXEC_PATH` **确实不存在**时
  才会 `del os.environ['RTT_EXEC_PATH']`，所以有效路径不会被清掉

**结论：不要修改 `rtconfig.py` 的 `EXEC_PATH`。** 只需要在 ENV shell 里构建即可，
这样工程在别的机器（装了 RT-Thread Studio 的）上也能直接用。

等价的手动方式（不进入 ENV 时）：

```bat
set RTT_EXEC_PATH=E:\sdk\rtthread\env-windows-v2.0.0\env-windows\tools\gnu_gcc\arm_gcc\mingw\bin
scons -j12
:: 或者
scons -j12 --exec-path="E:\sdk\rtthread\env-windows-v2.0.0\env-windows\tools\gnu_gcc\arm_gcc\mingw\bin"
```

---

## 4. 常用命令

在**工程目录**下执行：

| 命令 | 作用 |
| --- | --- |
| `scons` | 编译，产出 `<TARGET>.elf`（并依 `POST_ACTION` 生成 `.hex` / `.bin`） |
| `scons -j12` | 并行编译，推荐 |
| `scons -c` | 清理（也可直接删 `build/` 和 `.sconsign.dblite`） |
| `scons --target=mdk5` / `iar` | 生成 IDE 工程（本 BSP 用不到） |
| `scons --genconfig` | **反向**生成：由 `rtconfig.h` → `.config`，生成完即退出，不编译 |
| `scons --useconfig=<file>` | 用指定配置文件覆盖 `.config` |
| `menuconfig` | 图形/文本配置界面，改 `.config` 并重新生成 `rtconfig.h` |
| `pkgs --update` | 下载/更新软件包 |
| `pkgs --upgrade` | 升级包索引 |
| `pkgs --list` | 列出已配置的包 |

已验证：`scons --genconfig` 会调用 `rt-thread/tools/genconf.py`，把 `rtconfig.h`
里每个 `#define` 写成 `CONFIG_X=y` 或 `CONFIG_X=<值>`，然后 `exit(0)`。

---

## 5. 本工作区的两个坑

### 5.1 `menuconfig` 在这里跑不起来

不要对 `projects/` 下的工程执行 `menuconfig`，原因有三：

1. 各工程的 `Kconfig` 里有 `source "$PKGS_DIR/Kconfig"`，但
   `projects/<工程>/packages/Kconfig` **并不存在** → menuconfig 直接报错退出。
2. `scons --menuconfig` 在 `rt-thread/tools/building.py` 里被
   `if platform.system() != 'Windows'` 挡住，**在 Windows 上是空操作**。
3. 就算能跑，`menuconfig` 会用 `.config` 覆盖 `rtconfig.h`；而像
   `PKG_AHT10_I2C_BUS_NAME` 这类符号在 `libraries/M55_Config/Kconfig` 里位于
   `if PKG_USING_AHT10_SAMPLE`（默认 `n`）之下，**会被静默丢掉**，导致编译失败。

正确做法：**直接维护 `rtconfig.h`**，改完用 `scons --genconfig` 把 `.config` 同步回来。

```bash
# rtconfig.h 是权威，.config 是派生物
vim rtconfig.h
scons --genconfig
```

### 5.2 首次编译很慢

LVGL 的 `.o` 会被写进**共享的** `libraries/components/lvgl_9.2.0/` 源码树
（`variant_dir` 对 `Glob()` 的结果不生效）。不同工程若 `rtconfig.h` 不同
（例如 `RT_USING_CACHE`、`LVGL_USING_DEMOS`、`BSP_LVGL_DRAW_BUF_LINES` 不同），
编译命令行就不同 → 每次切换工程都会触发 LVGL 全量重编（约 276 个文件）。
另外 `romfs.c` 这类几 MB 的大数组在 `-O0`（`BUILD='debug'`）下编译也很慢。

---

## 6. 工程目录结构速查

一个可构建的 RT-Thread 工程通常包含：

```
<工程>/
├── SConstruct          # scons 入口：定义 TARGET、env、拉起各 SConscript
├── SConscript          # 根 SConscript：扫描子目录，含 SConscript 的都纳入
├── rtconfig.py         # 工具链/编译选项（EXEC_PATH、CFLAGS、LFLAGS、POST_ACTION）
├── rtconfig.h          # ★ 功能开关（由 .config 生成，或手写后 --genconfig 反向同步）
├── .config             # menuconfig 产物
├── Kconfig             # 工程级 Kconfig，source 了 rt-thread / libraries 的 Kconfig
├── board/              # board.c/h、linker_scripts/link.ld
├── applications/       # 用户代码（子目录自带 SConscript 即自动编译）
├── packages/           # 软件包（本工作区已全部内置，无需 pkgs --update）
├── rt-thread   -> junction
├── libraries   -> junction     # 由 mklinks.bat 创建，被 .gitignore 忽略
├── libs        -> junction     # 所以每个工程都要跑一次 mklinks.bat
└── tools       -> junction
```

**junction 必须先建好**，否则 `RTT_ROOT` 找不到、库 SConscript 全部失效：

```bat
cd <工程目录> && mklinks.bat
```

（`mklinks.bat` 内部是 `mklink /J`，Windows 上创建目录联接。）

---

## 7. 与 `projects/start.py` 的关系

`projects/start.py` 是工作区自带的一键「编译 + 烧录」脚本，它的编译步骤就是：

```python
subprocess.run(["scons", f"-j{int(os.cpu_count() * 4.0/5)}"],
               cwd=self.projectMainDir, check=True)
```

然后用 `self.projectMainDir.rglob("rt*thread*.hex")` 找到 hex 并调 OpenOCD 烧写。

因为它是 scons 的**子进程**，只要在 ENV shell 里运行就同样能继承 `RTT_EXEC_PATH`。
所以 **`rtconfig.py` 保持不动，`start.py` 也能正常工作**。

用法（需 `pydantic` / `pydantic-cli`）：

```bat
python start.py <OpenOCD安装目录> --projectMainDir <工程目录>
```

> 注意：不要为了凑 `Debug/rtthread.elf` 而在 `POST_ACTION` 里加拷贝步骤——
> 那会让 `start.py` 的 hex glob 拿到两个候选，而它取的是 `[0]`。
