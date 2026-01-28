/**
 * RISC-V bootloader
 * Memory Map:
 *   UART:  0x10000000
 *   SRAM:  0x80000000
 * 
 */

#include <stdint.h>

// =========================================================
// Peripheral Base Addresses
// =========================================================
#define UART_BASE   0x10000000
#define SRAM_BASE   0x80000000

// =========================================================
// UART Registers
// =========================================================
#define UART_RX_DATA    (*(volatile uint32_t*)(UART_BASE + 0x00))
#define UART_RX_AVAIL   (*(volatile uint32_t*)(UART_BASE + 0x04))
#define UART_TX_DATA    (*(volatile uint32_t*)(UART_BASE + 0x08))
#define UART_TX_BUSY    (*(volatile uint32_t*)(UART_BASE + 0x0C))

// =========================================================
// SPI Registers
// =========================================================
#define SPI_BASE      0x20000000 // Adjust to your actual MMIO base
#define SPI_TX_DATA   ((volatile uint32_t*)(SPI_BASE + 0x00))
#define SPI_RX_DATA   ((volatile uint32_t*)(SPI_BASE + 0x04))
#define SPI_CS_CTRL   ((volatile uint32_t*)(SPI_BASE + 0x08))
#define SPI_CLK_DIV   ((volatile uint32_t*)(SPI_BASE + 0x0C))
#define SPI_BUSY      ((volatile uint32_t*)(SPI_BASE + 0x10))

struct fw_dyn {
    long magic;
    long version;
    long next_addr;
    long next_mode;
    long options;
    long boot_hart;
};

// Placed in XIP due to 'const' and link.ld
const struct fw_dyn fw_dyn_data = {
    0x4942534f, // Magic "OSBI"
    2,          // Version
    0x80400000, // Next Addr (Linux)
    1,          // Next Mode (Supervisor)
    0,          // Options
    0           // Boot Hart
};

void uart_puts(char* str) {
    while (*str) {
        while (UART_TX_BUSY);
        UART_TX_DATA = *str++;
    }
}

// Small helper to print a single character
void uart_putc(char c) {
    while (UART_TX_BUSY); // Wait for UART to be ready
    UART_TX_DATA = c;
}

// Prints a byte as two hex characters (e.g., 0x4A -> "4A")
void uart_put_hex8(uint8_t b) {
    const char *hex_chars = "0123456789ABCDEF";
    uart_putc(hex_chars[(b >> 4) & 0x0F]);
    uart_putc(hex_chars[b & 0x0F]);
}

// Prints a 32-bit word as hex (useful for addresses)
void uart_put_hex32(uint32_t w) {
    for (int i = 3; i >= 0; i--) {
        uart_put_hex8((w >> (i * 8)) & 0xFF);
    }
}

uint8_t spi_xfer(uint8_t data) {
    while (*SPI_BUSY & 0x1); // Wait if busy
    *SPI_TX_DATA = data;     // Kick transfer
    
    // Add small delay to allow busy bit to assert
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");
    asm volatile ("nop");

    while (*SPI_BUSY & 0x1); // Wait for completion
    return (uint8_t)(*SPI_RX_DATA);
}

void set_cs(int level) {
    *SPI_CS_CTRL = level & 0x1;
}

uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t res;
    
    set_cs(0);
    spi_xfer(0x40 | cmd);
    spi_xfer((arg >> 24) & 0xFF);
    spi_xfer((arg >> 16) & 0xFF);
    spi_xfer((arg >> 8)  & 0xFF);
    spi_xfer(arg & 0xFF);
    spi_xfer(crc);

    // Poll for response (up to 8 tries)
    // The card pulls MISO high (0xFF) until it has a response
    for(int i = 0; i < 8; i++) {
        res = spi_xfer(0xFF);
        if ((res & 0x80) == 0) return res;
    }
    set_cs(1);
    return res; 
}

int sd_read_blocks(uint32_t start_block, uint32_t num_blocks, uint8_t *dst) {
    for (uint32_t i = 0; i < num_blocks; i++) {
        set_cs(0);
        
        // CMD17 = Read Single Block
        if (sd_cmd(17, start_block + i, 0xFF) != 0x00) {
            set_cs(1);
            return -1; // Error
        }

        // Wait for Data Token (0xFE)
        uint16_t timeout = 0xFFFF;
        while (spi_xfer(0xFF) != 0xFE && timeout--);
        
        if (timeout == 0) { set_cs(1); return -2; }

        // Read the 512-byte sector
        for (int b = 0; b < 512; b++) {
            *dst++ = spi_xfer(0xFF);
        }

        // Skip the 2-byte CRC
        spi_xfer(0xFF); 
        spi_xfer(0xFF);
        
        set_cs(1);
        spi_xfer(0xFF); // Provide 8 extra clocks for card cleanup
        
        // Progress indicator: Print a dot every 128 blocks (64KB)
        if ((i % 128) == 0) {
            uart_putc('.');
        }
    }
    uart_puts(" Done.\n\r");
    return 0;
}

int main() {
    uart_puts("\n\rRISC-V Bootloader v2.0\n\r");
    *SPI_CLK_DIV = 200;

    // 1. Power-on sequence
    set_cs(1);
    for(int i=0; i<10; i++) spi_xfer(0xFF); // 80 clocks

    // 2. Init Card
    if (sd_cmd(0, 0, 0x95) != 0x01){ 
        uart_puts("SD Card not found\n\r");
        while(1); // Reset
    }
    
    // CMD8
    if (sd_cmd(8, 0x1AA, 0x87) != 0x01) {
            uart_puts("CMD8 Failed\n\r");
    } else {
            // Read R7
            uint32_t r7 = 0;
            r7 |= (uint32_t)spi_xfer(0xFF) << 24;
            r7 |= (uint32_t)spi_xfer(0xFF) << 16;
            r7 |= (uint32_t)spi_xfer(0xFF) << 8;
            r7 |= (uint32_t)spi_xfer(0xFF);
            
            set_cs(1);
            spi_xfer(0xFF); // Cleanup
    }
    
    // Wait for card to exit idle
    int retries = 10000;
    while(retries-- > 0) {
        sd_cmd(55, 0, 0xFF);
        if (sd_cmd(41, 0x40000000, 0xFF) == 0x00) break;
    }
    
    if (retries <= 0) {
            uart_puts("Timeout waiting for card to exit idle\n\r");
            while(1);
    }

    // 3. Switch to High Speed
    *SPI_CLK_DIV = 4;
    uart_puts("SD Init Done. Reading Header...\n\r");

    // Read Block 0 (Header)
    uint8_t header_buf[512];
    if (sd_read_blocks(0, 1, header_buf) != 0) {
        uart_puts("Failed to read header\n\r");
        while(1);
    }

    // Parse Header
    // 0x00: Jump Address (4 bytes)
    // 0x04: Num Transfers (4 bytes)
    // 0x08: Entry 0 Start LBA (4 bytes)
    // ...

    uint32_t jump_addr = *(uint32_t*)(header_buf + 0);
    uint32_t num_transfers = *(uint32_t*)(header_buf + 4);

    uart_puts("Jump Addr: "); uart_put_hex32(jump_addr); uart_puts("\n\r");
    uart_puts("Transfers: "); uart_put_hex32(num_transfers); uart_puts("\n\r");

    uint32_t entry_offset = 8;
    for (uint32_t i = 0; i < num_transfers; i++) {
        uint32_t start_lba = *(uint32_t*)(header_buf + entry_offset);
        uint32_t end_lba   = *(uint32_t*)(header_buf + entry_offset + 4);
        uint32_t dest_addr = *(uint32_t*)(header_buf + entry_offset + 8);
        
        uint32_t num_blocks = end_lba - start_lba + 1;

        uart_puts("Loading LBA: "); uart_put_hex32(start_lba);
        uart_puts(" -> "); uart_put_hex32(dest_addr);
        uart_puts(" ("); uart_put_hex32(num_blocks); uart_puts(" blocks)\n\r");

        if (sd_read_blocks(start_lba, num_blocks, (uint8_t*)dest_addr) != 0) {
            uart_puts("Load Failed!\n\r");
            while(1);
        }

        // Verify first 16 bytes
        uart_puts("Verifying first 16 bytes: ");
        uint8_t* p = (uint8_t*)dest_addr;
        for(int k=0; k<16; k++) {
            uart_put_hex8(p[k]);
            uart_putc(' ');
        }
        uart_puts("\n\r");

        entry_offset += 12;
    }

    uart_puts("Booting...\n\r");
    
    // Calculate FDT Address: RAM Base (0x80000000) + 128MB (0x08000000) - 32MB (0x02000000)
    // = 0x86000000
    uint32_t fdt_addr = 0x86000000;
    uint32_t hartid = 0;

    // Jump to the loaded code with arguments
    // a0 = hartid
    // a1 = fdt_addr
    // a2 = &fw_dyn_data
    asm volatile (
        "mv a0, %0\n"
        "mv a1, %1\n"
        "mv a2, %2\n"
        "jr %3\n"
        : 
        : "r"(hartid), "r"(fdt_addr), "r"(&fw_dyn_data), "r"(jump_addr)
        : "a0", "a1", "a2", "memory"
    );

    // Should not reach here
    while(1);
}