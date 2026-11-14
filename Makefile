# =============================================================
#  VUKANG OS - Makefile
# =============================================================
CC      := gcc
AS      := nasm
LD      := ld

CFLAGS  := -m32 -std=gnu11 -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
           -fno-builtin -nostdlib -mgeneral-regs-only -fno-omit-frame-pointer \
           -O2 -Wall -Wextra -Iinclude
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

BUILD   := build
KERNEL  := $(BUILD)/vukang.kernel
ISO     := $(BUILD)/vukang-os.iso
DISK    := $(BUILD)/disk.img

C_SRC   := $(shell find kernel drivers fs db shell lib -name '*.c')
ASM_SRC := $(shell find boot kernel -name '*.asm')
OBJ     := $(patsubst %.c,$(BUILD)/%.o,$(C_SRC)) $(patsubst %.asm,$(BUILD)/%.asm.o,$(ASM_SRC))

.PHONY: all iso run run-serial disk clean test

all: $(KERNEL)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC   $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  AS   $<"
	@$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJ) linker.ld
	@echo "  LD   $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJ)
	@grub-file --is-x86-multiboot $@ && echo "  OK   multiboot kernel ready"

iso: $(KERNEL)
	@mkdir -p $(BUILD)/iso/boot/grub
	@cp $(KERNEL) $(BUILD)/iso/boot/vukang.kernel
	@cp grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	@grub-mkrescue -o $(ISO) $(BUILD)/iso >/dev/null 2>&1
	@echo "  ISO  $(ISO)"

disk:
	@mkdir -p $(BUILD)
	@test -f $(DISK) || (dd if=/dev/zero of=$(DISK) bs=1M count=16 status=none && echo "  DISK $(DISK) (16 MiB)")

# Graphical window (VGA) + serial log on terminal
run: iso disk
	qemu-system-i386 -m 128M -cdrom $(ISO) -drive file=$(DISK),format=raw,index=0,media=disk -boot d -serial stdio

# Headless: shell over the serial port (no window needed)
run-serial: iso disk
	qemu-system-i386 -m 128M -cdrom $(ISO) -drive file=$(DISK),format=raw,index=0,media=disk -boot d -nographic

test: iso
	@bash scripts/test.sh

clean:
	rm -rf $(BUILD)
