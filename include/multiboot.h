#ifndef VK_MULTIBOOT_H
#define VK_MULTIBOOT_H
#include <stdint.h>
#define MULTIBOOT_MAGIC 0x2BADB002
typedef struct {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
} __attribute__((packed)) multiboot_info_t;
typedef struct {
    uint32_t size;
    uint32_t addr_lo, addr_hi, len_lo, len_hi;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_t;
#endif
