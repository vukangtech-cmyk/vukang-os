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

# ---- user programs (ring 3), embedded into the kernel image ----
UCFLAGS := -m32 -std=gnu11 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
           -nostdlib -mgeneral-regs-only -O2 -Wall -Wextra -Iuser
USER_PROGS := hello counter attack_read attack_write attack_exec attack_cli attack_vga attack_syscall spin
USER_ELFS  := $(patsubst %,$(BUILD)/user/%.elf,$(USER_PROGS))
PROG_ASM   := $(BUILD)/user/programs.asm
OBJ        += $(BUILD)/user/programs.asm.o

.PHONY: all iso run run-serial disk clean test splash font versions run-version run-serial-version iso-version test-version clean-versions

all: $(KERNEL)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC   $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  AS   $<"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/user/crt0.o: user/crt0.c user/ulib.h
	@mkdir -p $(dir $@)
	@$(CC) $(UCFLAGS) -c $< -o $@

$(BUILD)/user/%.elf: user/%.c user/ulib.h user/user.ld $(BUILD)/user/crt0.o
	@mkdir -p $(dir $@)
	@echo "  USER $<"
	@$(CC) $(UCFLAGS) -c $< -o $(BUILD)/user/$*.o
	@$(LD) -m elf_i386 -T user/user.ld -nostdlib -o $@ $(BUILD)/user/crt0.o $(BUILD)/user/$*.o

$(PROG_ASM): $(USER_ELFS)
	@echo "  GEN  $@"
	@{ echo "section .rodata"; echo "align 4"; echo "global user_programs"; echo "user_programs:"; \
	   for p in $(USER_PROGS); do echo "  dd name_$$p, data_$$p, end_$$p - data_$$p"; done; \
	   echo "  dd 0, 0, 0"; \
	   for p in $(USER_PROGS); do echo "name_$$p: db \"$$p\", 0"; echo "align 16"; \
	     echo "data_$$p: incbin \"$(BUILD)/user/$$p.elf\""; echo "end_$$p:"; done; } > $@

$(BUILD)/user/programs.asm.o: $(PROG_ASM)
	@$(AS) $(ASFLAGS) $< -o $@

# binary assets (font + boot splash) are pulled in with incbin
ASSETS := $(wildcard assets/font/*.vkfont assets/splash/*.vkimg)
$(BUILD)/kernel/assets.asm.o: kernel/assets.asm $(ASSETS)

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

# Regenerate the splash layers / console font from their sources (needs Pillow)
splash:
	@python3 tools/mksplash.py
font:
	@python3 tools/mkfont.py

# Graphical window (VGA) + serial log on terminal
run: iso disk
	qemu-system-i386 -m 128M -cdrom $(ISO) -drive file=$(DISK),format=raw,index=0,media=disk -boot d -serial stdio

# Headless: shell over the serial port (no window needed)
run-serial: iso disk
	qemu-system-i386 -m 128M -cdrom $(ISO) -drive file=$(DISK),format=raw,index=0,media=disk -boot d -nographic

test: iso
	@bash scripts/test.sh

# Any released version side by side (scripts/version.sh), e.g.
#   make versions                 list tags
#   make run-version V=v0.1.0     run that version (own worktree + disk)
V ?= latest
versions:
	@bash scripts/version.sh list
run-version:
	@bash scripts/version.sh run $(V)
run-serial-version:
	@bash scripts/version.sh run-serial $(V)
iso-version:
	@bash scripts/version.sh iso $(V)
test-version:
	@bash scripts/version.sh test $(V)
clean-versions:
	@bash scripts/version.sh clean

clean:
	rm -rf $(BUILD)
	@git worktree prune 2>/dev/null || true
