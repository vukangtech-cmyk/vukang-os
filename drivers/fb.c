/* VUKANG OS - drivers/fb.c : linear framebuffer driver
 *
 * GRUB switches the screen to a graphics mode requested in the Multiboot
 * header (1024x768x32) and tells us where the framebuffer is. We only
 * accept direct-colour RGB modes of 16, 24 or 32 bits per pixel; anything
 * else falls back to the classic VGA text mode.
 */
#include "fb.h"
#include "multiboot.h"

fb_t fb;

void fb_probe(void *mbinfo) {
    multiboot_info_t *mb = mbinfo;
    fb.present = false;
    if (!mb || !(mb->flags & MB_INFO_FRAMEBUFFER) || mb->fb_type != 1) return;
    if (mb->fb_addr >> 32) return;                          /* must be addressable in 32-bit */
    if (mb->fb_bpp != 32 && mb->fb_bpp != 24 && mb->fb_bpp != 16) return;
    if (mb->fb_width < 640 || mb->fb_height < 480 || mb->fb_width > 4096 || mb->fb_height > 4096) return;
    if (mb->fb_pitch < mb->fb_width * (mb->fb_bpp / 8)) return;
    uint64_t size = (uint64_t)mb->fb_pitch * mb->fb_height;
    if ((uint64_t)(uint32_t)mb->fb_addr + size > 0x100000000ull) return;
    fb.phys = (uint32_t)mb->fb_addr; fb.size = (uint32_t)size;
    fb.base = (uint8_t *)fb.phys;                           /* paging is still off */
    fb.width = mb->fb_width; fb.height = mb->fb_height;
    fb.pitch = mb->fb_pitch; fb.bpp = mb->fb_bpp;
    fb.present = true;
}

void fb_set_base(uint32_t virt) { fb.base = (uint8_t *)virt; }

void fb_write_row(uint32_t x, uint32_t y, const uint32_t *px, uint32_t n) {
    if (!fb.present || y >= fb.height || x >= fb.width) return;
    if (x + n > fb.width) n = fb.width - x;
    uint8_t *row = fb.base + y * fb.pitch;
    if (fb.bpp == 32) {
        volatile uint32_t *d = (uint32_t *)row + x;
        for (uint32_t i = 0; i < n; i++) d[i] = px[i];
    } else if (fb.bpp == 24) {
        volatile uint8_t *d = row + x * 3;
        for (uint32_t i = 0; i < n; i++, d += 3) { d[0] = px[i]; d[1] = px[i] >> 8; d[2] = px[i] >> 16; }
    } else {
        volatile uint16_t *d = (uint16_t *)row + x;
        for (uint32_t i = 0; i < n; i++)
            d[i] = (uint16_t)(((px[i] >> 8) & 0xF800) | ((px[i] >> 5) & 0x07E0) | ((px[i] >> 3) & 0x001F));
    }
}

void fb_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb) {
    static uint32_t line[4096];
    if (w > 4096) w = 4096;
    for (uint32_t i = 0; i < w; i++) line[i] = rgb;
    for (uint32_t j = 0; j < h; j++) fb_write_row(x, y + j, line, w);
}
