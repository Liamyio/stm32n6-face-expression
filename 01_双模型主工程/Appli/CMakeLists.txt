cmake_minimum_required(VERSION 3.22)

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()
message("Build type: " ${CMAKE_BUILD_TYPE})

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
set(STM32_MCU_FLAGS "-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse")

# Linker script
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(STM32_LINKER_SCRIPT STM32N647XX_ROMxspi2_RAMxspi1.ld)
else()
    set(STM32_LINKER_SCRIPT STM32N647XX_LRUN.ld)
endif()
set(STM32_LINKER_OPTION)

# Project name
set(CMAKE_PROJECT_NAME ExprRecognition_Appli)
project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM)

# 确保 ASM 也有 MCU 标志
set(CMAKE_ASM_FLAGS "${STM32_MCU_FLAGS} -x assembler-with-cpp -MMD -MP")

add_executable(${CMAKE_PROJECT_NAME})

# Include CubeMX-generated base config (HAL, app sources, etc.)
include("mx-generated.cmake")

set(CMAKE_CXX_COMPILER_FORCED "true")

# ---- Extra compiler definitions ----
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    ARM_MATH_CM55
    LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6
    LL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL
    LL_ATON_SW_FALLBACK=1
)

# ---- stedgeai 平台头文件 ----
set(STEDGEAI_DIR "E:/ST/STedgeAi/4.0")
set(STEDGEAI_MW  "${STEDGEAI_DIR}/Middlewares/ST/AI")
set(STEDGEAI_ATON "${STEDGEAI_MW}/Npu/ll_aton")
set(STEDGEAI_DUMMY_APP "${STEDGEAI_DIR}/Utilities/windows/targets/stm32/resources/dummy_app")

# ---- Extra include paths ----
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/expr_net
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/face_net
    ${CMAKE_CURRENT_SOURCE_DIR}/NPU
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32_MW_CAMERA
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32_MW_ISP
    ${STEDGEAI_MW}/Inc
    ${STEDGEAI_MW}/Npu/ll_aton
    ${STEDGEAI_MW}/Npu/Devices/STM32N6xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_CAMERA
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_CAMERA/sensors/imx335
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_CAMERA/sensors
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/isp/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/evision/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/IMX335
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/IMX335/STM32_IMX335
)

# ---- Model source files ----
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # 表情识别
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/expr_net/network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/expr_net/stai_network.c
    # 人脸检测
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/face_net/network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/face_net/stai_network.c
    # 摄像头驱动
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_camera.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/bsp_bus_stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/MX_DCMIPP_ClockConfig.c
    # 摄像头中间件
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_CAMERA/cmw_camera.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_CAMERA/cmw_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_CAMERA/sensors/cmw_imx335.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_CAMERA/sensors/imx335/imx335.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_CAMERA/sensors/imx335/imx335_reg.c
    # BSP 驱动 (仅顶层 BSP wrapper, 不包含STM32_IMX335底层——已在Middlewares中有)
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/IMX335/imx335.c
    # ISP 中间件 (不包含 isp_conf_template.c — 与 cmw_camera 冲突)
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/isp/Src/isp_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/isp/Src/isp_algo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/isp/Src/isp_services.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/isp/Src/isp_cmd_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/isp/Src/isp_tool_com.c
    # ST AI 内部实现 (__ll_aton_stai_init/run/deinit)
    "${STEDGEAI_MW}/Npu/ll_aton/ll_aton_stai_internal.c"
    # STM32N6 设备特定 (MCU/NPU cache)
    "${STEDGEAI_MW}/Npu/Devices/STM32N6xx/mcu_cache.c"
    "${STEDGEAI_MW}/Npu/Devices/STM32N6xx/npu_cache.c"
    # (ll_aton_runtime.c 在 libnpu_runtime.a 中，不需重复编译)
)

# ---- 追加 HAL 驱动 (UART + RAMCFG + CACHEAXI) ----
target_sources(STM32_Drivers PRIVATE
    C:/Users/liam/STM32Cube/Repository/STM32Cube_FW_N6_V1.3.0/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.c
    C:/Users/liam/STM32Cube/Repository/STM32Cube_FW_N6_V1.3.0/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ramcfg.c
    C:/Users/liam/STM32Cube/Repository/STM32Cube_FW_N6_V1.3.0/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cacheaxi.c
    C:/Users/liam/STM32Cube/Repository/STM32Cube_FW_N6_V1.3.0/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dcmipp.c
    C:/Users/liam/STM32Cube/Repository/STM32Cube_FW_N6_V1.3.0/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma2d.c
    C:/Users/liam/STM32Cube/Repository/STM32Cube_FW_N6_V1.3.0/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c.c
)

# ---- NPU Runtime (rebuild for the real STM32N6 target) ----
add_library(npu_runtime_stm32n6 STATIC
    "${STEDGEAI_ATON}/ecloader.c"
    "${STEDGEAI_ATON}/ll_aton.c"
    "${STEDGEAI_ATON}/ll_aton_cipher.c"
    "${STEDGEAI_ATON}/ll_aton_dbgtrc.c"
    "${STEDGEAI_ATON}/ll_aton_debug.c"
    "${STEDGEAI_ATON}/ll_aton_lib.c"
    "${STEDGEAI_ATON}/ll_aton_lib_sw_operators.c"
    "${STEDGEAI_ATON}/ll_aton_profiler.c"
    "${STEDGEAI_ATON}/ll_aton_reloc_callbacks.c"
    "${STEDGEAI_ATON}/ll_aton_reloc_network.c"
    "${STEDGEAI_ATON}/ll_aton_rt_main.c"
    "${STEDGEAI_ATON}/ll_aton_runtime.c"
    "${STEDGEAI_ATON}/ll_aton_util.c"
    "${STEDGEAI_ATON}/ll_sw_float.c"
    "${STEDGEAI_ATON}/ll_sw_integer.c"
    "${STEDGEAI_DUMMY_APP}/stedgeai_hsp_stubs.c"
)
target_link_libraries(npu_runtime_stm32n6 PUBLIC stm32cubemx)
target_include_directories(npu_runtime_stm32n6 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/NPU
    ${STEDGEAI_MW}/Inc
    ${STEDGEAI_ATON}
    ${STEDGEAI_MW}/Npu/Devices/STM32N6xx
)
target_compile_definitions(npu_runtime_stm32n6 PRIVATE
    ARM_MATH_CM55
    LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6
    LL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL
    LL_ATON_RT_MODE=LL_ATON_RT_POLLING
    LL_ATON_SW_FALLBACK=1
)
target_compile_options(npu_runtime_stm32n6 PRIVATE
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-unused-variable
)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${STEDGEAI_MW}/Lib/GCC/ARMCortexM55"
)
target_link_libraries(${CMAKE_PROJECT_NAME}
    npu_runtime_stm32n6
    "${STEDGEAI_MW}/Lib/GCC/ARMCortexM55/NetworkRuntime1201_CM55_GCC.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/evision/Lib/libn6-evision-awb_gcc.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/STM32_MW_ISP/evision/Lib/libn6-evision-st-ae_gcc.a"
    m
)

# newlib-nano omits %f formatting unless the float formatter is pulled in.
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -u _printf_float
)

# Keep the flashable image synchronized with every successful ELF link.
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${CMAKE_PROJECT_NAME}>.hex
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex"
    VERBATIM
)

# Remove wrong libob.a
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()

# ---- 编译选项 ----
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-unused-variable
)
