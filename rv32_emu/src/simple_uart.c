// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

#include "defs.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static struct termios term_config;
static pthread_mutex_t fifo_lock;
static bool print_type = false;

uint8_t simple_uart_rx_fifo_pop(simple_uart_td *uart) {
    if (uart->fifo_write_ptr - uart->fifo_read_ptr > 0)
        return uart->fifo_buf[uart->fifo_read_ptr++ % 16];

    return uart->fifo_buf[uart->fifo_read_ptr % 16];
}

void simple_uart_rx_fifo_push(simple_uart_td *uart, uint8_t val) {
    // drop oldest on full
    if (uart->fifo_write_ptr - uart->fifo_read_ptr >= 16)
        uart->fifo_read_ptr++;
    uart->fifo_buf[uart->fifo_write_ptr++ % 16] = val;
}

int64_t simple_uart_get_size(simple_uart_td *uart) {
    return uart->fifo_write_ptr - uart->fifo_read_ptr;
}

// reads stdin in raw mode and pushes bytes to the rx fifo
void *simple_uart_rx_thread(void *ptr) {
    simple_uart_td *uart = (simple_uart_td *)ptr;
    unsigned char temp_buf;
    while (true) {
        temp_buf = getchar();
        if (temp_buf == 1) { // Ctrl+A
            printf("CTRL+A pressed, dying uwu\n");
            exit(0);
        } else if (temp_buf == 22) { // Ctrl+V ?
            print_type = !print_type;
        }

        pthread_mutex_lock(&fifo_lock);
        simple_uart_rx_fifo_push(uart, temp_buf);
        pthread_mutex_unlock(&fifo_lock);
    }
    return NULL;
}

void simple_uart_revert(void) {
    term_config.c_lflag |= (ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(0, TCSANOW, &term_config);
}

void simple_uart_init(simple_uart_td *uart) {
    pthread_t thread_id;
    pthread_mutex_init(&fifo_lock, NULL);
    tcgetattr(0, &term_config);
    struct termios raw_config = term_config;
    raw_config.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(0, TCSANOW, &raw_config);
    atexit(simple_uart_revert);
    pthread_create(&thread_id, NULL, simple_uart_rx_thread, uart);
}

bool simple_uart_update(simple_uart_td *uart) {
    return simple_uart_get_size(uart) > 0;
}

// 0x00=rx_data 0x04=rx_avail 0x08=tx_data 0x0c=tx_busy
bool simple_uart_bus_access_func(simple_uart_td *uart, uint32_t addr, bus_access access,
                                 uint32_t *val, uint8_t len) {

    if (access == bus_write) {
        switch (addr) {
        case 0x08:
            if (print_type)
                printf("%d\n", (uint8_t)*val);
            else
                putchar((uint8_t)*val);
            fflush(stdout);
            break;
        default:
            break;
        }
    } else {
        switch (addr) {
        case 0x00: // rx data
            pthread_mutex_lock(&fifo_lock);
            *val = simple_uart_rx_fifo_pop(uart);
            pthread_mutex_unlock(&fifo_lock);
            break;
        case 0x04: // rx_data_new
            *val = (simple_uart_get_size(uart) > 0) ? 1 : 0;
            break;
        case 0x0c:
            *val = 0; // never busy
            break;
        default:
            break;
        }
    }
    return true;
}
