#include <stdint.h>
#include <stdbool.h>
#include "types.h"

#define extract_bits(val, a, b) (((val) >> (b)) & ((1 << ((a)-(b)+1)) - 1))
#define sext_bits(a) (0xffffffff << (a))

bool soc_init(soc_td *soc, char *sbi_file, char *linux_file, char *disk_img, char *fdt_file);
void soc_run(soc_td *soc);
bool soc_bus_access_func(soc_td *soc, uint32_t addr, bus_access access, uint32_t *val, uint8_t len);

bool csr_read(csr_td *csr, priv_level_td privilege, uint32_t addr,uint32_t *val);
bool csr_write(csr_td *csr, priv_level_td privilege, uint32_t addr, uint32_t val);
void csr_write_mip(csr_td *csr, bool val, uint8_t bit);

void core_init(core_td *core, soc_td *soc_ptr);
bool core_fetch(core_td *core);
void core_decode(core_td *core);
void core_execute(core_td *core);
void core_prepare_sync_trap(core_td *core, trap_cause_exception cause, uint32_t tval);
void core_process_interrupts(core_td *core, bool mei, bool sei, bool mti, bool msi);
void core_dump(core_td *core);

bool simple_uart_bus_access_func(simple_uart_td *uart, uint32_t addr, bus_access access, uint32_t *val, uint8_t len);
void simple_uart_init(simple_uart_td *uart);
bool simple_uart_update(simple_uart_td *uart);

bool clint_bus_access_func(clint_td *clint, uint32_t addr, bus_access access, uint32_t* val, uint8_t len);
void clint_update(clint_td *clint, bool* msi, bool* mti);

bool plic_bus_access_func(plic_td *plic, uint32_t addr, bus_access access, uint32_t* val, uint8_t len);
void plic_update(plic_td *plic, bool* mei, bool* sei);
void plic_update_pending(plic_td *plic, uint32_t id, bool val);
void plic_init(plic_td *plic);

// void mmu_init(mmu_td *mmu);
// void mmu_update_satp(mmu_td *mmu, uint32_t satp);
// void mmu_flush_tlb(mmu_td *mmu);
// bool mmu_translate(core_td *core, uint32_t vaddr, int access, uint32_t *paddr);
// bool mmu_access(core_td *core, uint32_t addr, int access, uint32_t *val, uint8_t len);

void helper_write_from_file(char *fname, uint8_t *mem_ptr, uint64_t size);
uint64_t helper_get_file_size(char *fname);