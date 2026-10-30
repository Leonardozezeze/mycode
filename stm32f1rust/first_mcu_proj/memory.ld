ENTRY(reset_handle)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_start_of_stack = ORIGIN(RAM) + LENGTH(RAM);
_min_stack_size = 0x400;
_min_heap_size  = 0x400;

SECTIONS
{
    .text : {
        LONG(_start_of_stack);      /* 偏移 0x00：初始 SP */
        KEEP(*(.isr_vector));       /* 偏移 0x04 起：向量表 */
        *(.text*)
        . = ALIGN(4);
    } > FLASH

    .rodata : {
        . = ALIGN(4);
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .data : {
        . = ALIGN(4);
        _sidata = LOADADDR(.data);
        _sdata  = .;
        *(.data*)
        . = ALIGN(4);
        _edata  = .;
    } > RAM AT> FLASH

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    .ram_usage_check (NOLOAD) : {
        . = ALIGN(8);
        . = . + _min_stack_size;
        . = . + _min_heap_size;
        . = ALIGN(8);
    } > RAM

    ASSERT(_ebss + _min_stack_size + _min_heap_size <= ORIGIN(RAM) + LENGTH(RAM),
           "RAM overflow: .bss + stack + heap exceeds RAM size")
}

INCLUDE "device_STM32F103.x"