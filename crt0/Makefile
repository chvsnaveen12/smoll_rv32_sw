# Toolchain
CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

# Flags
ARCH_FLAGS = -march=rv32i_zicsr -mabi=ilp32
CFLAGS = $(ARCH_FLAGS) -Os -g -Wall -ffreestanding -nostdlib -nostartfiles
ASFLAGS = $(ARCH_FLAGS)
LDFLAGS = -T link.ld -nostdlib

# Files
TARGET = firmware
# OBJS = crt0_test.o
OBJS = crt0.o main.o

# Default target
all: $(TARGET).hex $(TARGET).bin $(TARGET).lst

# Compile assembly
%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

# Compile C
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(TARGET).elf: $(OBJS) link.ld
	$(CC) $(ARCH_FLAGS) $(LDFLAGS) $(OBJS) -o $@

# Generate hex (for simulation - addresses start at 0 for sim_flash)
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) --change-addresses -0x40000000 -O verilog $< $@

# Generate binary (padded to 4KB boundary)
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@# Pad to next 4KB boundary
	@SIZE=$$(stat -c%s $@); \
	PADDED=$$(( ((SIZE + 16383) / 16384) * 16384 )); \
	truncate -s $$PADDED $@
	@echo "Padded $(TARGET).bin to $$(stat -c%s $@) bytes"

# Generate disassembly listing
$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $@

# Copy hex to soc directory for simulation
install: $(TARGET).hex
	cp $(TARGET).hex ../soc/firmware.hex

# Clean
clean:
	rm -f *.o *.elf *.hex *.bin *.lst

.PHONY: all clean install
