cmake_minimum_required(VERSION 3.22)

#
# CubeMX + Rust FFI 模板：c_src 侧只负责把 C/C++ 代码编译成 .o，
# 并打包成静态库 libMXTemplate.a。
#
# 最终固件的链接（启动文件、链接脚本、Rust 目标文件、C 静态库）
# 全部由工程根目录的 build.rs 交给 rustc 完成。
#
# 该文件只生成一次，CubeMX 重新生成代码时不会被覆盖，可自由修改。
#

# C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# C++ 支持（App 目录下的用户代码）
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# 生成 compile_commands.json，便于 clangd / IDE 索引
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# 项目：C + C++ + 汇编（启动文件）
project(MXTemplate LANGUAGES C CXX ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

# 只生成静态库，不做任何链接。
# 静态库中就是各源文件编译出的 .o，由 build.rs 负责最终链接。
add_library(${PROJECT_NAME} STATIC)

# CubeMX 生成的启动文件、HAL 驱动、main.c 等
add_subdirectory(cmake/stm32cubemx)

# 用户区：如仍有 C/C++ 代码，放在 App/ 下即可（目录不存在时跳过）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/App)
    add_subdirectory(App)
endif()

# CubeMX 模板遗留项：避免链接 libob.a
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# 头文件路径与宏定义（stm32cubemx 是 INTERFACE 库）
target_link_libraries(${PROJECT_NAME}
    stm32cubemx
)
