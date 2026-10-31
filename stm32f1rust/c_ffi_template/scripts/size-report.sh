#!/usr/bin/env bash
# 打印 FLASH / RAM 占用汇总。
#
# 用法: scripts/size-report.sh <elf> [链接脚本]
#
#   FLASH = text + data      （.data 的加载副本在 Flash 里）
#   RAM   = data + bss       （含链接脚本预留的 heap/stack）
#
# 区域大小从链接脚本的 MEMORY 段读取，换芯片只改 .ld 即可。

set -euo pipefail

elf=${1:?用法: size-report.sh <elf> [linker-script]}
ld=${2:-c_src/STM32F103xx_FLASH.ld}
size_cmd=${SIZE:-arm-none-eabi-size}

flash_kb=$(sed -n 's/^FLASH[^:]*:.*LENGTH *= *\([0-9]*\)K.*/\1/p' "$ld")
ram_kb=$(sed -n 's/^RAM[^:]*:.*LENGTH *= *\([0-9]*\)K.*/\1/p' "$ld")

if [ -z "$flash_kb" ] || [ -z "$ram_kb" ]; then
    echo "size-report: 无法从 $ld 解析 FLASH/RAM 大小" >&2
    exit 1
fi

read -r text data bss _ < <("$size_cmd" "$elf" | tail -n 1)

flash=$((text + data))
ram=$((data + bss))
flash_max=$((flash_kb * 1024))
ram_max=$((ram_kb * 1024))

percent() {
    awk -v used="$1" -v total="$2" 'BEGIN { printf "%.1f", used * 100 / total }'
}

printf 'Memory usage:\n'
printf '  FLASH: %6d / %6d B (%5s%%)\n' "$flash" "$flash_max" "$(percent "$flash" "$flash_max")"
printf '  RAM:   %6d / %6d B (%5s%%)\n' "$ram" "$ram_max" "$(percent "$ram" "$ram_max")"
