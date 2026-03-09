// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

#include "defs.h"
#include "memory.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define RISCOF
#define RISCOF_CYCLES 20000

int main(int argc, char **argv) {
#ifdef RISCOF
    uint32_t sig_begin, sig_end;

    if (argc != 7)
        exit(-1);

    sig_begin = strtoul(argv[5], (char **)NULL, 16);
    sig_end = strtoul(argv[6], (char **)NULL, 16);

    soc_td soc;
    memset(&soc, 0, sizeof(soc_td));

    soc_init(&soc, argv[1], argv[2], argv[3], argv[4]);
    for (int i = 0; i < RISCOF_CYCLES; i++)
        soc_run(&soc);

    for (int i = sig_begin; i < sig_end; i += 4) {
        uint32_t temp;
        soc_bus_access_func(&soc, i, bus_read, &temp, 4);
        printf("%08x\n", temp);
    }

#else
    if (argc != 5) {
        printf("Usage: ./emu opensbi_file linux_file disk_img fdt_file\n");
        exit(-1);
    }

    soc_td soc;
    memset(&soc, 0, sizeof(soc_td));

    printf("SOC INIT\n");
    soc_init(&soc, argv[1], argv[2], argv[3], argv[4]);
    while (true)
        soc_run(&soc);
#endif
}
