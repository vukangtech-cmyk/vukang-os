/* VUKANG OS - drivers/vga.c : 80x25 VGA text mode driver with scrolling */
#include "vga.h"
#include "io.h"
#include "string.h"

#define W 80
#define H 25
static volatile uint16_t *const VGA = (uint16_t *)0xB8000;
static int row, col;
static uint8_t color = VGA_LGREY | (VGA_BLACK << 4);

static void cursor_update(void) {
    uint16_t pos = row * W + col;
    outb(0x3D4, 14); outb(0x3D5, pos >> 8);
    outb(0x3D4, 15); outb(0x3D5, pos & 0xFF);
}
void vga_set_color(uint8_t fg, uint8_t bg) { color = fg | (bg << 4); }
uint8_t vga_get_color(void) { return color; }
void vga_clear(void) {
    for (int i = 0; i < W * H; i++) VGA[i] = (uint16_t)' ' | (color << 8);
    row = col = 0; cursor_update();
}
void vga_init(void) { vga_clear(); }
static void scroll(void) {
    for (int i = 0; i < W * (H - 1); i++) VGA[i] = VGA[i + W];
    for (int i = W * (H - 1); i < W * H; i++) VGA[i] = (uint16_t)' ' | (color << 8);
    row = H - 1;
}
void vga_putc(char c) {
    if (c == '\n') { col = 0; row++; }
    else if (c == '\r') col = 0;
    else if (c == '\b') { if (col > 0) col--; }
    else if (c == '\t') { col = (col + 4) & ~3; }
    else { VGA[row * W + col] = (uint16_t)(uint8_t)c | (color << 8); col++; }
    if (col >= W) { col = 0; row++; }
    if (row >= H) scroll();
    cursor_update();
}
