# smoll_rv32_sw

Software stack for the [smoll_rv32](../smoll_rv32) core. OpenSBI, Linux, device tree, BusyBox, crt0, and an RV32 emulator.

## Repo layout

- `opensbi/` OpenSBI M-mode firmware
- `linux/` Linux kernel
- `dts/` device tree source and blob
- `busybox/` BusyBox userspace
- `crt0/` baremetal C runtime and test firmware
- `configs/` kernel and BusyBox config files
- `packer/` image packer
- `rv32_emu/` RV32 emulator

## Toolchain

You need a RISC-V rv32ima toolchain. Build it from the GNU source:

- `riscv-gnu-toolchain` configured for `--with-arch=rv32ima_zicsr_zifencei --with-abi=ilp32`

OpenSBI builds with `riscv64-linux-gnu-` and Linux builds with `riscv32-unknown-linux-gnu-`.

## Build

Device tree:

```
cd dts
make
```

OpenSBI:

```
cd opensbi
make CROSS_COMPILE=riscv64-linux-gnu- PLATFORM=generic PLATFORM_RISCV_XLEN=32 PLATFORM_RISCV_ISA=rv32ima_zicsr_zifencei PLATFORM_RISCV_ABI=ilp32
```

Linux:

```
cd linux
make ARCH=riscv CROSS_COMPILE=riscv32-unknown-linux-gnu- -j$(nproc)
```

Emulator:

```
cd rv32_emu
make
```

## Outputs

- `opensbi/build/platform/generic/firmware/fw_dynamic.bin`
- `linux/arch/riscv/boot/Image`
- `dts/dts.dtb`
- `rv32_emu/emu`

Use these with [smoll_rv32](../smoll_rv32). Prebuilt copies are also checked in there under `prebuilt/`.

## EMU/RTL diff

Compare emulator and RTL traces:

```
diff -y <(sed 's/\[EMU\] //' emu.log) <(sed 's/\[RTL\] //' rtl.log)
```

## License

MIT. See `LICENSE`.
