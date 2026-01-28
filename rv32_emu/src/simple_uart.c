#include <stdio.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "defs.h"

// Internal state for UART
static struct termios term_config;
static pthread_mutex_t fifo_lock;
static simple_uart_td *global_uart_ptr = NULL;

static bool print_type = false;

uint8_t simple_uart_rx_fifo_pop(simple_uart_td *uart){
    if(uart->fifo_write_ptr - uart->fifo_read_ptr > 0)
        return uart->fifo_buf[uart->fifo_read_ptr++ % 16];
    
    return uart->fifo_buf[uart->fifo_read_ptr % 16];
}

void simple_uart_rx_fifo_push(simple_uart_td *uart, uint8_t val){
    if(uart->fifo_write_ptr - uart->fifo_read_ptr < 16)
        uart->fifo_buf[uart->fifo_write_ptr++ % 16] = val;
    else{
        uart->fifo_buf[uart->fifo_write_ptr++ % 16] = val;
        uart->fifo_read_ptr++;
    }
}

int64_t simple_uart_get_size(simple_uart_td *uart){
    return uart->fifo_write_ptr - uart->fifo_read_ptr;
}

void* simple_uart_rx_thread(void* ptr){
    simple_uart_td *uart = (simple_uart_td*)ptr;
    unsigned char temp_buf;
    while(true){
        temp_buf = getchar();
        if(temp_buf == 1){ // Ctrl+A
            printf("CTRL+A pressed, dying uwu\n");
            exit(0);
        }
        else if(temp_buf == 22){ // Ctrl+V ?
            print_type = !print_type;
        }
        
        pthread_mutex_lock(&fifo_lock);
        if (uart) simple_uart_rx_fifo_push(uart, temp_buf);
        pthread_mutex_unlock(&fifo_lock);
    }
    return NULL;
}

void simple_uart_revert(void){
    term_config.c_lflag |= (ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(0, TCSANOW, &term_config);
}

void simple_uart_init(simple_uart_td *uart){
    pthread_t thread_id;
    global_uart_ptr = uart;
    pthread_mutex_init(&fifo_lock, NULL);
    tcgetattr(0, &term_config);
    struct termios raw_config = term_config;
    raw_config.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(0, TCSANOW, &raw_config);
    atexit(simple_uart_revert);
    pthread_create(&thread_id, NULL, simple_uart_rx_thread, uart);
}

bool simple_uart_update(simple_uart_td *uart){
    // The interrupt fires whenever there is even a single RX character available.
    if (simple_uart_get_size(uart) > 0) {
        return true;
    }
    return false;
}

bool simple_uart_bus_access_func(simple_uart_td *uart, uint32_t addr, bus_access access, uint32_t* val, uint8_t len){
    // 0 for rx data, 0x04 for rx_data_new, 0x08 for tx_data write, 0x0c for tx_busy
    
    if(access == bus_write){
        switch(addr){
            case 0x08: // tx_data write
                if(print_type)
                    printf("%d\n", (uint8_t)*val);
                else
                    putchar((uint8_t)*val);
                fflush(stdout);
                break;
            default:
                // Invalid write
                break;
        }
    }
    else { // Read
        switch(addr){
            case 0x00: // rx data
                pthread_mutex_lock(&fifo_lock);
                *val = simple_uart_rx_fifo_pop(uart);
                pthread_mutex_unlock(&fifo_lock);
                break;
            case 0x04: // rx_data_new
                *val = (simple_uart_get_size(uart) > 0) ? 1 : 0;
                break;
            case 0x0c: // tx_busy
                *val = 0; // Always ready
                break;
            default:
                // Invalid read
                break;
        }
    }
    return true;
}

// #include <stdio.h>
// #include <pthread.h>
// #include <termios.h>
// #include <unistd.h>
// #include <string.h>
// #include <stdlib.h>
// #include "defs.h"
// #include "types.h"

// // Internal state for UART (assuming single instance for now)
// static struct termios term_config;
// static pthread_mutex_t fifo_lock;
// static simple_uart_td *global_uart_ptr = NULL; // To access from thread if needed, or pass via arg

// // Register offsets
// #define R_RBR_DLL 0
// #define R_IER_DLM 1
// #define R_IIR 2
// #define R_LCR 3
// #define R_MCR 4
// #define R_LSR 5
// #define R_MSR 6
// #define R_SCR 7

// #define W_THR_DLL 0
// #define W_IER_DLM 1
// #define W_FCR 2
// #define W_LCR 3
// #define W_MCR 4
// #define W_SCR 7

// #define UART_LSR 0x60
// #define UART_MSR 0

// static bool print_type = false;
// // extern temp; // From original, not sure what this is. Removing for now.

// uint8_t uart_rx_fifo_pop(simple_uart_td *uart){
//     if(uart->fifo_write_ptr - uart->fifo_read_ptr > 0)
//         return uart->fifo_buf[uart->fifo_read_ptr++ % 16];
    
//     return uart->fifo_buf[uart->fifo_read_ptr % 16];
// }

// void uart_rx_fifo_push(simple_uart_td *uart, uint8_t val){
//     if(uart->fifo_write_ptr - uart->fifo_read_ptr < 16)
//         uart->fifo_buf[uart->fifo_write_ptr++ % 16] = val;
//     else{
//         uart->fifo_buf[uart->fifo_write_ptr++ % 16] = val;
//         uart->fifo_read_ptr++;
//     }
// }

// int64_t uart_get_size(simple_uart_td *uart){
//     return uart->fifo_write_ptr - uart->fifo_read_ptr;
// }

// void* uart_rx_thread(void* ptr){
//     simple_uart_td *uart = (simple_uart_td*)ptr;
//     unsigned char temp_buf;
//     while(true){
//         temp_buf = getchar();
//         if(temp_buf == 1){ // Ctrl+A
//             printf("CTRL+A pressed, dying uwu\n");
//             exit(0);
//         }
//         else if(temp_buf == 22){ // Ctrl+V ?
//             print_type = !print_type;
//         }
        
//         pthread_mutex_lock(&fifo_lock);
//         if (uart) uart_rx_fifo_push(uart, temp_buf);
//         pthread_mutex_unlock(&fifo_lock);
//     }
//     return NULL;
// }

// void uart_revert(void){
//     term_config.c_lflag |= (ECHO | ICANON | IEXTEN | ISIG);
//     tcsetattr(0, TCSANOW, &term_config);
// }

// void simple_uart_init(simple_uart_td *uart){
//     pthread_t thread_id;
//     // memset(uart, 0, sizeof(uart_td)); // Already zeroed in soc_init probably
//     global_uart_ptr = uart;
//     pthread_mutex_init(&fifo_lock, NULL);
//     tcgetattr(0, &term_config);
//     struct termios raw_config = term_config;
//     raw_config.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
//     tcsetattr(0, TCSANOW, &raw_config);
//     atexit(uart_revert);
//     pthread_create(&thread_id, NULL, uart_rx_thread, uart);
// }

// bool simple_uart_update(simple_uart_td *uart){
//     int64_t rx_int_threshold = 1;
//     uint8_t rx_int;
//     uint8_t tx_int;
    
//     uart->fcr &= ~(0b100);

//     if(uart->fcr & 0b10){
//         uart->fifo_read_ptr = uart->fifo_write_ptr;
//         uart->fcr &= ~(0b10);
//     }

//     rx_int = uart_get_size(uart) >= rx_int_threshold ? 1 : 0;

//     rx_int = rx_int & uart->ier;
//     tx_int = (uart->thre_int << 1) & uart->ier;

//     if(rx_int)
//         uart->iir = 0b0100;
//     else if(tx_int)
//         uart->iir = 0b0010;
//     else
//         uart->iir = 1;

//     uart->iir |= uart->fcr & 1 ? 0b11000000 : 0;
//     return !(uart->iir & 1);
// }

// bool simple_uart_bus_access_func(simple_uart_td *uart, uint32_t addr, bus_access access, uint32_t* val, uint8_t len){
//     if(len != 1){
//         printf("Uart doesn't support multi-byte bus access, dying uwu\n");
//         exit(-1);
//     }

//     if(access == bus_write){
//         switch(addr){
//             case W_THR_DLL:
//                 if(uart->lcr & 0x80){
//                     uart->dll = *val;
//                 }
//                 else{
//                     if(print_type)
//                         printf("%d\n", (uint8_t)*val);
//                     else
//                         putchar((uint8_t)*val);
//                     fflush(stdout);
//                     uart->thre_int = 1;
//                 }
//                 break;
//             case W_IER_DLM:
//                 if(uart->lcr & 0x80){
//                     uart->dlm = *val;
//                 }
//                 else{
//                     uart->ier = *val;
//                 }
//                 break;
//             case W_FCR:
//                 uart->fcr = *val;
//                 break;
//             case W_LCR:
//                 uart->lcr = *val;
//                 break;
//             case W_MCR:
//                 uart->mcr = *val;
//                 break;
//             case W_SCR:
//                 uart->scr = *val;
//                 break;
//             default:
//                 printf("Write Invalid UART Addr: 0x%08x, dying uwu\n", addr);
//                 exit(-1);
//                 break;
//         }
//     }
//     else { // Read
//         switch(addr){
//             case R_RBR_DLL:
//                 if(uart->lcr & 0x80){
//                     *val = uart->dll;
//                 }
//                 else{
//                     pthread_mutex_lock(&fifo_lock);
//                     *val = uart_rx_fifo_pop(uart);
//                     pthread_mutex_unlock(&fifo_lock);
//                 }
//                 break;
//             case R_IER_DLM:
//                 if(uart->lcr & 0x80){
//                     *val = uart->dlm;
//                 }
//                 else{
//                     *val = uart->ier;
//                 }
//                 break;
//             case R_IIR:
//                 *val = uart->iir;
//                 uart->thre_int = 0;
//                 break;
//             case R_LCR:
//                 *val = uart->lcr;
//                 break;
//             case R_MCR:
//                 *val = uart->mcr;
//                 break;
//             case R_LSR:
//                 *val = UART_LSR | (uart_get_size(uart) > 0 ? 1 : 0);
//                 break;
//             case R_MSR:
//                 *val = UART_MSR;
//                 break;
//             case R_SCR:
//                 *val = uart->scr;
//                 break;
//             default:
//                 printf("Read Invalid UART Addr: 0x%08x, dying uwu\n", addr);
//                 exit(-1);
//                 break;
//         }
//     }
//     return true;
// }

