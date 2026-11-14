#ifndef VK_FB_H
#define VK_FB_H
/* VUKANG OS - linear framebuffer (graphics mode set by GRUB via Multiboot) */
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    bool     present;
    uint32_t phys, size;                 /* physical address and size in bytes */
    uint8_t *base;                       /* current (virtual) address */
    uint32_t width, height, pitch, bpp;
} fb_t;
extern fb_t fb;

#define FB_VIRT 0xC0000000u              /* kernel-only window when fb is above the identity map */

void fb_probe(void *mbinfo);             /* before paging: read Multiboot framebuffer info */
void fb_set_base(uint32_t virt);         /* called by paging_init after mapping */
/* Draw n pixels (0x00RRGGBB) at (x, y). Handles 32/24/16 bpp. */
void fb_write_row(uint32_t x, uint32_t y, const uint32_t *px, uint32_t n);
void fb_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb);
#endif
