// #include <cstdint>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<fcntl.h>
#include <sys/mman.h>
#include<unistd.h>
#include<string.h>
// #include"types.h"
#include"defs.h"
#include"memory.h"
#include "types.h"

bool soc_bus_access_func(soc_td *soc, uint32_t addr, bus_access access, uint32_t *val, uint8_t len){
    if(RAM_BASE <= addr && addr + len <= RAM_BASE + RAM_SIZE){
        // getchar();
        switch (access){
            case bus_read:
            case bus_fetch:
                memcpy(val, &soc->ram[addr - RAM_BASE], len);
                return true;
            case bus_write:
                memcpy(&soc->ram[addr - RAM_BASE], val, len);
                return true;
            default:
                return false;
        }
    }

    else if(ROM_BASE <= addr && addr + len <= ROM_BASE + ROM_SIZE){
        // getchar();
        switch (access){
            case bus_read:
            case bus_fetch:
                memcpy(val, &soc->rom[addr - ROM_BASE], len);
                return true;
            default:
                return false;
        }
    }

    else if(CLINT_BASE <= addr && addr + len <= CLINT_BASE + CLINT_SIZE){
        return clint_bus_access_func(&soc->clint, addr - CLINT_BASE, access, val, len);
    }

    else if(PLIC_BASE <= addr && addr + len <= PLIC_BASE + PLIC_SIZE)
        return plic_bus_access_func(&soc->plic, addr - PLIC_BASE, access, val, len);

    // else if(BLK_BASE <= addr && addr + len <= BLK_BASE + BLK_SIZE)
    //     return blk_bus_access_func(&soc->blk, addr - BLK_BASE, access, val, len);

    else if(UART_BASE <= addr && addr + len <= UART_BASE + UART_SIZE){
        return simple_uart_bus_access_func(&soc->uart, addr - UART_BASE, access, val, len);
        // return uart_bus_access_func(&soc->uart, addr - UART_BASE, access, val, len);
    }
    return false;
}


bool soc_init(soc_td *soc, char *sbi_file, char *linux_file, char *disk_img, char *fdt_file){
    uint32_t boot_rom[] = {
        0x00000297,                 /* 1:  auipc  t0, %pcrel_hi(fw_dyn) */
        0x02828613,                 /*     addi   a2, t0, %pcrel_lo(1b) */
        0xf1402573,                 /*     csrr   a0, mhartid  */
        0x0202a583,                 /*     lw     a1, 32(t0) */
        0x0182a283,                 /*     lw     t0, 24(t0) */
        0x00028067,                 /*     jr     t0 */
        RAM_BASE  ,                 /* start: .dword */
        0x00000000,
        FDT_ADDR  ,                 /* fdt_laddr: .dword */
        0x00000000,
                                    /* fw_dyn: */
        0x4942534f,                 /* OSBI */
        // 0x00000000,
        0x00000002,                 /* Version */
        // 0x00000000,
        LINUX_ADDR,                 /* Next stage addr */
        // 0x00000000,
        0x00000001,                 /* Next stage mode (Supervisor) */
        // 0x00000000,
        0x00000000,                 /* OpenSBI options */
        // 0x00000000,
        0x00000000,                 /* Boot Hart */
        // 0x00000000
    };

    memset(soc, 0, sizeof(soc_td));

    // ROM init
    soc->rom = (uint8_t*)malloc(ROM_SIZE * sizeof(uint8_t));
    if(soc->rom == NULL){
        printf("malloc for ROM failed\n");
        exit(-1);
    }
    memcpy(soc->rom, boot_rom, sizeof(boot_rom));

    // RAM init
    soc->ram = (uint8_t*)malloc(RAM_SIZE * sizeof(uint8_t));
    if(soc->ram == NULL){
        printf("malloc for RAM failed\n");
        exit(-1);
    }
    helper_write_from_file(sbi_file, soc->ram, RAM_SIZE);
    helper_write_from_file(linux_file, &soc->ram[LINUX_ADDR - RAM_BASE], RAM_SIZE - (LINUX_ADDR - RAM_BASE));
    helper_write_from_file(fdt_file, &soc->ram[FDT_ADDR - RAM_BASE], RAM_SIZE - (FDT_ADDR - RAM_BASE));

    // blk_init(&soc->blk, disk_img, soc);
    simple_uart_init(&soc->uart);
    soc->clint.mtime = 0xffffffffffffffffUL;  // Prevent timer interrupts until mtimecmp is set
    core_init(&soc->core, soc);
    return true;
}

uint64_t count = 0;
void soc_run(soc_td *soc){
    bool mei = 0, sei = 0, msi = 0, mti = 0;
    core_td alpha = soc->core;

    // if (soc->core.csr.satp != 0) {
    //     printf("SATP is non-zero (0x%08x). System frozen (paging not supported in this mode).\n", soc->core.csr.satp);
    //     while(1) {
    //         sleep(1);
    //     }
    // }

    if(core_fetch(&soc->core)){
        core_decode(&soc->core);
        core_execute(&soc->core);
    }
    
    bool simple_uart_irq = simple_uart_update(&soc->uart);
    
    plic_update_pending(&soc->plic, 4, simple_uart_irq);
    plic_update(&soc->plic, &mei, &sei);

    clint_update(&soc->clint, &msi, &mti);
    // core_process_interrupts(&soc->core, 0, 0, 0, 0);
    core_process_interrupts(&soc->core, mei, sei, mti, msi);

    // if(soc->core.cycle == 7892795){
    // if(soc->core.cycle == 21711816){
    // if(soc->core.cycle == 7890674){
    // if(count++ == 306223){
    // IT"S SAME TILL THIS
    // if(soc->core.pc == 0x80012354){ Cycle: 289448ish
    // if(soc->core.cycle >= 661570){ // Cycle: 289542
    // if(soc->core.cycle >= 100000000){ // Cycle: 289542
    //     core_dump(&soc->core);
    //     sleep(1);
    //     // getchar();
    // }
    // if (soc->core.cycle > 1103486600) {                                                                                                                                                       
    // if (soc->core.csr.mepc == 0x000453ac) {                                                                                                                                                       
    // if (soc->core.cycle >= 1018248173) {
    //     core_dump(&soc->core);
    //     // sleep(1);
    // }
    // if(soc->core.cycle == 1018248400){
    //     exit(0);
    // }
}