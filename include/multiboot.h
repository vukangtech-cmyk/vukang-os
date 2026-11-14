#ifndef VK_MULTIBOOT_H
#define VK_MULTIBOOT_H
#include <stdint.h>
#define MULTIBOOT_MAGIC 0x2BADB002
#define MB_INFO_CMDLINE     (1u << 2)
#define MB_INFO_MMAP        (1u << 6)
#define MB_INFO_FRAMEBUFFER (1u << 12)
typedef struct {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr, config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t fb_addr;
    uint32_t fb_pitch, fb_width, fb_height;
    uint8_t  fb_bpp, fb_type;               /* type 1 = RGB linear framebuffer, 2 = EGA text */
    uint8_t  fb_red_pos, fb_red_size, fb_green_pos, fb_green_size, fb_blue_pos, fb_blue_size;
} __attribute__((packed)) multiboot_info_t;
typedef struct {
    uint32_t size;
    uint32_t addr_lo, addr_hi, len_lo, len_hi;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_t;
#endif
