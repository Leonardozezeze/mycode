cmake_minimum_required(VERSION 3.20)

# ==============================================================================
# 目标平台与工具链
# ==============================================================================
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(TOOLCHAIN_PREFIX "arm-none-eabi")
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}-objcopy)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}-objdump)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}-size)
set(CMAKE_LINKER       ${TOOLCHAIN_PREFIX}-ld)

# 交叉编译：禁止 CMake 运行链接测试（嵌入式目标无法运行可执行程序）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER_WORKS 1)

project(MyStm32F407Project C CXX ASM)

# ==============================================================================
# 构建类型与 IDE 支持
# ==============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type: Debug or Release" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 为 clangd / LSP 生成 compile_commands.json

# ==============================================================================
# CPU 与 FPU 编译标志
# ==============================================================================
set(CPU_FLAGS  -mcpu=cortex-m4 -mthumb)
set(FPU_FLAGS  -mfloat-abi=hard -mfpu=fpv4-sp-d16)

# ==============================================================================
# 全局宏定义（所有 target 共享）
# ==============================================================================
add_compile_definitions(STM32F407xx USE_HAL_DRIVER)

# ==============================================================================
# 源文件收集（使用 CONFIGURE_DEPENDS 使增量构建自动检测新增/删除文件）
# ==============================================================================

# ---- 启动文件 ----
set(STARTUP_ASM CMSIS/Start_Linker_SVD/startup_stm32f407xx.s)
set_source_files_properties(${STARTUP_ASM} PROPERTIES LANGUAGE ASM)

# ---- 用户源码 (Core/Src: main/bsp/app + 中断 + MSP + syscalls) ----
# ---- 自己写的驱动 (Driver/Src) ----
file(GLOB_RECURSE USER_SOURCES
    CONFIGURE_DEPENDS
    "Core/Src/*.c"
    "Core/Src/*.cpp"
    "Driver/Src/*.c"
    "Driver/Src/*.cpp"
)

# ---- CMSIS + FreeRTOS ----
file(GLOB_RECURSE SYSTEM_SOURCES
    CONFIGURE_DEPENDS
    "CMSIS/Src/*.c"
    "Middlewares/FreeRTOS/Src/*.c"
    "Middlewares/FreeRTOS/portable/*.c"
)

# ---- CANopenNode 协议栈 + STM32 移植层 (Middlewares/CANopenNode_STM32/) ----
# 移植层用非递归 GLOB(顶层 .c),协议栈用 GLOB_RECURSE(含 301/303/.../storage)。
# 若移植层也用 GLOB_RECURSE,会把 CANopenNode/ 子目录里的协议栈源码重复匹配,导致重复符号。
file(GLOB_RECURSE CANOPEN_CORE_SOURCES
    CONFIGURE_DEPENDS
    "Middlewares/CANopenNode_STM32/CANopenNode/*.c"  # 协议栈本体 (301/303/.../storage)
)
file(GLOB CANOPEN_PORT_SOURCES
    CONFIGURE_DEPENDS
    "Middlewares/CANopenNode_STM32/*.c"              # STM32 移植:驱动 + 应用 + 对象字典 OD.c
)
set(CANOPEN_SOURCES ${CANOPEN_CORE_SOURCES} ${CANOPEN_PORT_SOURCES})

# ---- HAL 驱动库 ----
file(GLOB_RECURSE HAL_SOURCES
    CONFIGURE_DEPENDS
    "HAL/Src/*.c"
)

# ==============================================================================
# 头文件搜索路径
# ==============================================================================
set(CANOPEN_INCLUDE_DIRS
    Middlewares/CANopenNode_STM32/CANopenNode  # 协议栈根:源码按 301/303/... 相对引用头文件
    Middlewares/CANopenNode_STM32              # 移植层:CO_driver_target.h / OD.h / CO_app_STM32.h
)

set(COMMON_INCLUDE_DIRS
    CMSIS/Inc
    Middlewares/FreeRTOS/portable
    Middlewares/FreeRTOS/Inc
    HAL/Inc
    HAL/Inc/Legacy
    Core/Inc
    Driver/Inc
    ${CANOPEN_INCLUDE_DIRS}
)

# ==============================================================================
# 静态库 Target：系统库 (CMSIS + FreeRTOS + startup)
# ==============================================================================
add_library(SYS_LIB STATIC ${SYSTEM_SOURCES} ${STARTUP_ASM})
target_include_directories(SYS_LIB PUBLIC ${COMMON_INCLUDE_DIRS})
target_compile_options(SYS_LIB PRIVATE ${CPU_FLAGS} ${FPU_FLAGS} -g -Wall $<$<COMPILE_LANGUAGE:C>:-std=gnu99>)

# ==============================================================================
# 静态库 Target：HAL 驱动库
# ==============================================================================
add_library(HAL_LIB STATIC ${HAL_SOURCES})
target_include_directories(HAL_LIB PUBLIC ${COMMON_INCLUDE_DIRS})
target_compile_options(HAL_LIB PRIVATE ${CPU_FLAGS} ${FPU_FLAGS} -g -Wall $<$<COMPILE_LANGUAGE:C>:-std=gnu99>)
target_link_libraries(HAL_LIB PRIVATE SYS_LIB)

# ==============================================================================
# 静态库 Target：CANopenNode 协议栈
# ==============================================================================
add_library(CANOPEN_LIB STATIC ${CANOPEN_SOURCES})
target_include_directories(CANOPEN_LIB PUBLIC ${COMMON_INCLUDE_DIRS})
target_compile_options(CANOPEN_LIB PRIVATE ${CPU_FLAGS} ${FPU_FLAGS} -g -Wall $<$<COMPILE_LANGUAGE:C>:-std=gnu99>)
target_link_libraries(CANOPEN_LIB PUBLIC SYS_LIB HAL_LIB)

# ==============================================================================
# 可执行文件
# ==============================================================================
add_executable(${PROJECT_NAME}.elf ${USER_SOURCES})

# 编译选项
target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    ${FPU_FLAGS}
    -g
    -Wall
    -ffunction-sections
    -fdata-sections
    -fno-builtin
    $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++11 -fno-exceptions -fno-rtti>
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Release>:-Os>
)

# 链接库
target_link_libraries(${PROJECT_NAME}.elf PRIVATE SYS_LIB HAL_LIB CANOPEN_LIB)

# 链接脚本与链接选项
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/CMSIS/Start_Linker_SVD/STM32F407XX_FLASH.ld)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    ${FPU_FLAGS}
    -specs=nano.specs
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -T${LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_NAME}.map,--cref
)

# ==============================================================================
# 后处理：生成 .hex 和 .bin
# ==============================================================================
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generate HEX, BIN and print size info"
)
