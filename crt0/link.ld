/* Linker Script for RISC-V SoC
 * 
 * Memory Map:
 *   FLASH: 0x40000000 (Code + Read-only data)
 *   SRAM:  0x80000000 (Stack only, 128MB)
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x40000000, LENGTH = 64K
    SRAM  (rw)  : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    /* Code and read-only data in FLASH */
    .text : {
        *(.text.init)       /* Startup code first */
        *(.text .text.*)    /* All other code */
        *(.rodata .rodata.*)/* Read-only data */
        . = ALIGN(4);
    } > FLASH

    /* Global pointer for relaxed addressing */
    PROVIDE(__global_pointer$ = .);

    /* Data section - kept in FLASH (read-only) */
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
        . = ALIGN(4);
    } > FLASH

    /* BSS - in SRAM but we skip initialization */
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        . = ALIGN(4);
        __bss_end = .;
    } > SRAM

    /* Stack grows down from top of SRAM */
    __stack_top = ORIGIN(SRAM) + LENGTH(SRAM);

    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
