#ifndef VK_VGA_H
#define VK_VGA_H
/* VUKANG OS - screen console. Two back-ends behind one API:
 *   - framebuffer console (graphics mode, bitmap font, modern palette)
 *   - classic VGA text mode 80x25 at 0xB8000 (fallback)
 * Every character cell is kept in a shadow buffer, so the screen can be
 * redrawn at any time (after the boot animation, after a mode change). */
#include <stdint.h>
#include <stdbool.h>
enum vga_color {
    VGA_BLACK, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LGREY,
    VGA_DGREY, VGA_LBLUE, VGA_LGREEN, VGA_LCYAN, VGA_LRED, VGA_LMAGENTA, VGA_YELLOW, VGA_WHITE
};
void vga_init(void);                     /* picks framebuffer or text mode */
void vga_clear(void);
void vga_putc(char c);
void vga_set_color(uint8_t fg, uint8_t bg);
uint8_t vga_get_color(void);
int  vga_cols(void);
int  vga_rows(void);
bool vga_is_graphics(void);
void vga_write_at(int col, int row, const char *s, uint8_t attr);   /* no cursor move */
void vga_hold(bool on);                  /* hold: update cells only, draw nothing */
void vga_redraw(void);                   /* repaint every cell */
uint32_t vga_palette(int i);             /* 0x00RRGGBB of colour i */
/* font access for other graphics code (boot animation) */
int  vga_font_w(void);
int  vga_font_h(void);
const uint8_t *vga_glyph(uint8_t c);     /* rows of font_bpr bytes, MSB = left */
int  vga_font_bpr(void);
#endif
