MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 最基本的段定义 —— 仅让程序能跑，不做 BSS 清零 / data 搬运 */
SECTIONS
{
    .text : {
        KEEP(*(.vector_table))
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : { *(.data*) }   > RAM AT> FLASH
    .bss  : { *(.bss*)  }   > RAM
}