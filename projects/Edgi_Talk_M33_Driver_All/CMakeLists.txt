CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_PROCESSOR cortex-m33)
#SET(CMAKE_VERBOSE_MAKEFILE ON)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

SET(CMAKE_C_COMPILER "E:/sdk/rtthread/env-windows-v2.0.0/env-windows/tools/bin/../../tools/gnu_gcc/arm_gcc/mingw/bin/arm-none-eabi-gcc.exe")
SET(CMAKE_ASM_COMPILER "E:/sdk/rtthread/env-windows-v2.0.0/env-windows/tools/bin/../../tools/gnu_gcc/arm_gcc/mingw/bin/arm-none-eabi-gcc.exe")
SET(CMAKE_C_FLAGS " -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=softfp -ffunction-sections -fdata-sections -nostartfiles -g -Wall -pipe -O3 -gdwarf-2 -g")
SET(CMAKE_ASM_FLAGS " -c -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=softfp -ffunction-sections -fdata-sections -nostartfiles -x assembler-with-cpp -Wa,-mimplicit-it=thumb  -gdwarf-2")
SET(CMAKE_C_COMPILER_WORKS TRUE)

SET(CMAKE_CXX_COMPILER "E:/sdk/rtthread/env-windows-v2.0.0/env-windows/tools/bin/../../tools/gnu_gcc/arm_gcc/mingw/bin/arm-none-eabi-g++.exe")
SET(CMAKE_CXX_FLAGS " -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=softfp -ffunction-sections -fdata-sections -nostartfiles -g -Wall -pipe -O3 -gdwarf-2 -g")
SET(CMAKE_CXX_COMPILER_WORKS TRUE)

SET(CMAKE_OBJCOPY "E:/sdk/rtthread/env-windows-v2.0.0/env-windows/tools/bin/../../tools/gnu_gcc/arm_gcc/mingw/bin/arm-none-eabi-objcopy.exe")
SET(CMAKE_SIZE "E:/sdk/rtthread/env-windows-v2.0.0/env-windows/tools/bin/../../tools/gnu_gcc/arm_gcc/mingw/bin/arm-none-eabi-size.exe")

SET(CMAKE_EXE_LINKER_FLAGS " -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=softfp -ffunction-sections -fdata-sections -nostartfiles -Wl,--gc-sections,-Map=rtthread.map,-cref,-u,Reset_Handler -T ${CMAKE_SOURCE_DIR}/board/linker_scripts/link.ld")

SET(CMAKE_CXX_STANDARD 14)
PROJECT(rtthread C CXX ASM)
INCLUDE_DIRECTORIES(
	packages/aht10-latest/inc
	applications
	libraries/Common/board
	libraries/Common/board/ports/fal
	libraries/Common/board/ports/audio
	rt-thread/components/libc/compilers/common/include
	rt-thread/components/libc/compilers/newlib
	packages/CoreMark-latest
	rt-thread/libcpu/arm/common
	libraries/components/serial-memory/include
	rt-thread/components/drivers/audio
	rt-thread/components/drivers/include
	board
	libraries/HAL_Drivers
	libraries/HAL_Drivers/config
	rt-thread/components/fal/inc
	rt-thread/components/dfs/dfs_v1/include
	rt-thread/components/dfs/dfs_v1/filesystems/devfs
	rt-thread/components/dfs/dfs_v1/filesystems/elmfat
	rt-thread/components/dfs/dfs_v1/filesystems/romfs
	rt-thread/components/finsh
	.
	rt-thread/include
	rt-thread/libcpu/arm/cortex-m33
	libraries/components/Infineon_cmsis-latest/Core/Include
	libraries/components/Infineon_cmsis-latest/Core/Include/m-profile
	libraries/components/Infineon_core-lib-latest/include
	libraries/components/mtb-device-support-pse8xxgp/hal/include
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/include
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/third_party/ethernet/include
	libraries/components/mtb-device-support-pse8xxgp/pdl/devices/include
	libraries/components/mtb-device-support-pse8xxgp/pdl/devices/include/ip
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/third_party/COMPONENT_GFXSS
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/third_party/COMPONENT_GFXSS/vsi/dcnano8000
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/third_party/COMPONENT_GFXSS/vsi/gcnano
	libraries/components/async-transfer/include
	libraries/components/mtb-device-support-pse8xxgp/device-utils/syspm/include
	libraries/components/Infineon_retarget-io-latest
	libraries/components/ASRC/COMPONENT_CM33/inc
	libraries/components/mtb-srf/include
	libraries/components/mtb-srf/include/COMPONENT_NON_SECURE_DEVICE
	libraries/components/mtb-srf/include/COMPONENT_NON_SECURE_DEVICE/COMPONENT_MW_MTB_IPC
	libraries/components/mtb-ipc/include
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/bluetooth
	libraries/components/littlefs
	packages/lsm6ds3tr
	packages/optparse-latest
	rt-thread/components/libc/posix/io/epoll
	rt-thread/components/libc/posix/io/eventfd
	rt-thread/components/libc/posix/io/poll
	rt-thread/components/libc/posix/ipc
	libraries/Common/board/ports/display_panels
	packages/wavplayer-latest
	packages/wavplayer-latest/inc
)

ADD_DEFINITIONS(
	-DBLHS_SUPPORT
	-DCOMPONENT_55500
	-DCOMPONENT_55500A1
	-DCOMPONENT_APP_KIT_PSE84_EVAL_EPC2
	-DCOMPONENT_CM33
	-DCOMPONENT_CM33_0
	-DCOMPONENT_CYW55513_MOD_PSE84_SOM
	-DCOMPONENT_Debug
	-DCOMPONENT_GCC_ARM
	-DCOMPONENT_GFXSS
	-DCOMPONENT_HCI_UART
	-DCOMPONENT_MTB_DEVICE_SUPPORT
	-DCOMPONENT_MTB_HAL
	-DCOMPONENT_MW_ASYNC_TRANSFER
	-DCOMPONENT_MW_BT_FW_IFX_CYW55500A1
	-DCOMPONENT_MW_CMSIS
	-DCOMPONENT_MW_CORE_LIB
	-DCOMPONENT_MW_CORE_MAKE
	-DCOMPONENT_MW_MTB_DSL_PSE8XXGP
	-DCOMPONENT_MW_MTB_IPC
	-DCOMPONENT_MW_MTB_SRF
	-DCOMPONENT_MW_SE_RT_SERVICES_UTILS
	-DCOMPONENT_NON_SECURE_DEVICE
	-DCOMPONENT_PSE84
	-DCOMPONENT_SM
	-DCOMPONENT_SOFTFP
	-DCOMPONENT_WIFI_INTERFACE_SDIO
	-DCOMPONENT_wlbga_iPA_sLNA_ANT0_LHL_XTAL_IN
	-DCORE_NAME_CM33_0=1
	-DCYBSP_MCUBOOT_HEADER_SIZE=0x400
	-DCYBSP_WIFI_WL_HOSTWAKE_DRIVE_MODE=MTB_HAL_GPIO_DRIVE_OPENDRAINDRIVESLOW
	-DCYBSP_WIFI_WL_HOSTWAKE_INIT_STATE=WHD_TRUE
	-DCY_APPNAME_proj_cm33_ns
	-DCY_PDL_FLASH_BOOT
	-DCY_RETARGET_IO_CONVERT_LF_TO_CRLF
	-DCY_SUPPORTS_DEVICE_VALIDATION
	-DCY_TARGET_BOARD=APP_KIT_PSE84_EVAL_EPC2
	-DCY_WIFI_COUNTRY=WHD_COUNTRY_UNITED_STATES
	-DDEBUG
	-DFLASH_BOOT
	-DLFS_CONFIG=lfs_config.h
	-DPSE846GPS2DBZC4A
	-DRT_USING_LIBC
	-DRT_USING_NEWLIBC
	-DTARGET_APP_KIT_PSE84_EVAL_EPC2
	-DTRXV5
	-D_POSIX_C_SOURCE=1
	-D__RTTHREAD__
)

SET(PROJECT_SOURCES
	packages/aht10-latest/src/aht10.c
	applications/demo_key.c
	applications/main.c
	applications/demo_sdcard.c
	applications/demo_aht20.c
	applications/demo_adc.c
	applications/demo_lsm6ds3.c
	applications/demo_rtc.c
	applications/demo_hyperram.c
	applications/demo_audio.c
	applications/demo_flash_speed.c
	libraries/Common/board/ports/fal/fal_flash_port.c
	libraries/Common/board/ports/filesystem/mnt.c
	libraries/Common/board/ports/audio/drv_es8388.c
	libraries/Common/board/ports/drv_hyperam.c
	rt-thread/components/libc/compilers/common/cctype.c
	rt-thread/components/libc/compilers/common/cstdlib.c
	rt-thread/components/libc/compilers/common/cstring.c
	rt-thread/components/libc/compilers/common/ctime.c
	rt-thread/components/libc/compilers/common/cunistd.c
	rt-thread/components/libc/compilers/common/cwchar.c
	rt-thread/components/libc/compilers/newlib/syscalls.c
	packages/CoreMark-latest/core_matrix.c
	packages/CoreMark-latest/core_state.c
	packages/CoreMark-latest/core_list_join.c
	packages/CoreMark-latest/core_util.c
	packages/CoreMark-latest/core_portme.c
	packages/CoreMark-latest/core_main.c
	rt-thread/libcpu/arm/common/div0.c
	rt-thread/libcpu/arm/common/showmem.c
	libraries/components/serial-memory/source/mtb_serial_memory.c
	rt-thread/components/drivers/audio/audio.c
	rt-thread/components/drivers/audio/audio_pipe.c
	rt-thread/components/drivers/core/device.c
	rt-thread/components/drivers/i2c/i2c-bit-ops.c
	rt-thread/components/drivers/i2c/i2c_core.c
	rt-thread/components/drivers/i2c/i2c_dev.c
	rt-thread/components/drivers/ipc/completion.c
	rt-thread/components/drivers/ipc/dataqueue.c
	rt-thread/components/drivers/ipc/pipe.c
	rt-thread/components/drivers/ipc/ringblk_buf.c
	rt-thread/components/drivers/ipc/ringbuffer.c
	rt-thread/components/drivers/ipc/waitqueue.c
	rt-thread/components/drivers/ipc/workqueue.c
	rt-thread/components/drivers/misc/adc.c
	rt-thread/components/drivers/misc/pin.c
	rt-thread/components/drivers/mtd/mtd_nor.c
	rt-thread/components/drivers/rtc/rtc.c
	rt-thread/components/drivers/sdio/block_dev.c
	rt-thread/components/drivers/sdio/gpt.c
	rt-thread/components/drivers/sdio/mmc.c
	rt-thread/components/drivers/sdio/mmcsd_core.c
	rt-thread/components/drivers/sdio/sd.c
	rt-thread/components/drivers/sdio/sdio.c
	rt-thread/components/drivers/serial/serial.c
	board/board.c
	libraries/HAL_Drivers/drv_adc.c
	libraries/HAL_Drivers/drv_common.c
	libraries/HAL_Drivers/drv_gpio.c
	libraries/HAL_Drivers/drv_i2c.c
	libraries/HAL_Drivers/drv_i2s.c
	libraries/HAL_Drivers/drv_pdm.c
	libraries/HAL_Drivers/drv_rtc.c
	libraries/HAL_Drivers/drv_sdio.c
	libraries/HAL_Drivers/drv_soft_i2c.c
	libraries/HAL_Drivers/drv_uart.c
	rt-thread/components/fal/src/fal.c
	rt-thread/components/fal/src/fal_rtt.c
	rt-thread/components/fal/src/fal_flash.c
	rt-thread/components/fal/src/fal_partition.c
	rt-thread/components/dfs/dfs_v1/filesystems/devfs/devfs.c
	rt-thread/components/dfs/dfs_v1/filesystems/elmfat/dfs_elm.c
	rt-thread/components/dfs/dfs_v1/filesystems/elmfat/ff.c
	rt-thread/components/dfs/dfs_v1/filesystems/elmfat/ffunicode.c
	rt-thread/components/dfs/dfs_v1/filesystems/romfs/dfs_romfs.c
	rt-thread/components/dfs/dfs_v1/filesystems/romfs/romfs.c
	rt-thread/components/dfs/dfs_v1/src/dfs.c
	rt-thread/components/dfs/dfs_v1/src/dfs_file.c
	rt-thread/components/dfs/dfs_v1/src/dfs_fs.c
	rt-thread/components/dfs/dfs_v1/src/dfs_posix.c
	rt-thread/components/finsh/shell.c
	rt-thread/components/finsh/msh.c
	rt-thread/components/finsh/msh_parse.c
	rt-thread/components/finsh/cmd.c
	rt-thread/components/finsh/msh_file.c
	rt-thread/src/clock.c
	rt-thread/src/components.c
	rt-thread/src/idle.c
	rt-thread/src/ipc.c
	rt-thread/src/irq.c
	rt-thread/src/kservice.c
	rt-thread/src/mem.c
	rt-thread/src/memheap.c
	rt-thread/src/mempool.c
	rt-thread/src/object.c
	rt-thread/src/scheduler_up.c
	rt-thread/src/thread.c
	rt-thread/src/timer.c
	rt-thread/libcpu/arm/cortex-m33/cpuport.c
	rt-thread/libcpu/arm/cortex-m33/context_gcc.S
	rt-thread/libcpu/arm/cortex-m33/syscall_gcc.S
	rt-thread/libcpu/arm/cortex-m33/trustzone.c
	libraries/components/Infineon_retarget-io-latest/cy_retarget_io.c
	libraries/components/async-transfer/source/mtb_async_transfer.c
	libraries/components/mtb-device-support-pse8xxgp/device-utils/syspm/source/mtb_syspm_callbacks_scb_uart.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_clock.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_dma.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_dma_dmac.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_dma_dw.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_gpio.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_i2c.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_lptimer.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_rtc.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_sdhc.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_syspm.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_system.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_uart.c
	libraries/components/mtb-device-support-pse8xxgp/hal/source/mtb_hal_utils_impl.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/devices/source/cy_device.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/TOOLCHAIN_GCC_ARM/cy_syslib_ext.S
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_autanalog.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_autanalog_ac.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_autanalog_ctb.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_autanalog_dac.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_autanalog_ptc.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_autanalog_sar.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_axidmac.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_dma.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_gpio.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_ipc_drv.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_ipc_pipe.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_ipc_sema.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_pdm_pcm_v2.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_rram.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_rtc.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_scb_common.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_scb_i2c.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_scb_uart.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_sd_host.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_smif.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_smif_hb_flash.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_smif_memnum.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_smif_memslot.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_smif_sfdp.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_sysclk_v2.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_sysint_v2.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_syslib.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_syspm_pdcm.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_syspm_ppu.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_syspm_v4.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_systick_v2.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_systrimm.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/cy_tdm.c
	libraries/components/mtb-device-support-pse8xxgp/pdl/drivers/source/ppu_v1.c
	libraries/components/mtb-ipc/source/mtb_ipc.c
	libraries/components/mtb-srf/mtb_srf.c
	libraries/components/mtb-srf/mtb_srf_ipc.c
	libraries/components/mtb-srf/mtb_srf_pool.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg_protection.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg_qspi_memslot.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg_system.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/COMPONENT_CM33/COMPONENT_NON_SECURE_DEVICE/ns_system_pse84.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg_peripheral_clocks.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg_clocks.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/cybsp.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/COMPONENT_CM33/COMPONENT_NON_SECURE_DEVICE/ns_start_pse84.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg_peripherals.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/system_edge.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg_pins.c
	libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg.c
	libraries/components/littlefs/lfs_crc.c
	libraries/components/littlefs/lfs_util.c
	libraries/components/littlefs/dfs_lfs.c
	libraries/components/littlefs/lfs.c
	packages/lsm6ds3tr/lsm6ds3tr-c_reg.c
	packages/optparse-latest/optparse.c
	packages/wavplayer-latest/src/wavplayer_cmd.c
	packages/wavplayer-latest/src/wavplayer.c
	packages/wavplayer-latest/src/wavhdr.c
)

LINK_DIRECTORIES(
)

LINK_LIBRARIES(
	c
	m
)

ADD_EXECUTABLE(${CMAKE_PROJECT_NAME}.elf ${PROJECT_SOURCES})
ADD_CUSTOM_COMMAND(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD 
COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin COMMAND ${CMAKE_SIZE} ${CMAKE_PROJECT_NAME}.elf)