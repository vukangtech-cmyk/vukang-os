/* VUKANG OS - drivers/vga.c : screen console (framebuffer or VGA text)
 *
 * The console keeps a shadow buffer of character cells (char + colour).
 * Text mode copies cells to VGA memory at 0xB8000. Graphics mode draws
 * each cell with a 10x20 bitmap font (Terminus Bold, SIL OFL 1.1) into
 * the linear framebuffer, using a modern 16-colour palette.
 */
#include "vga.h"
#include "fb.h"
#include "io.h"
#include "string.h"

#define MAXC 256
#define MAXR 128
static uint16_t cells[MAXC * MAXR];
static uint16_t shown[MAXC * MAXR];     /* what is on screen now (damage tracking) */
#define STALE 0xFFFFu
static int cols = 80, rows = 25, row, col;
static uint8_t color = VGA_LGREY | (VGA_BLACK << 4);
static bool graphics, hold;
static volatile uint16_t *const TEXT = (uint16_t *)0xB8000;
static uint32_t ox, oy;                  /* top-left pixel of the text area */

/* ---- font (assets/font/console.vkfont, embedded by kernel/assets.asm) ---- */
extern const uint8_t asset_font[];
static int fw = 8, fh = 16, fbpr = 1;
static const uint8_t *glyphs;

static const uint32_t palette[16] = {
    0x080B10, 0x2F6FDB, 0x2EB872, 0x1BA6B8, 0xD14D4D, 0xA25BD9, 0xC28B2C, 0xC8D0DA,
    0x5C6773, 0x6CA8FF, 0x5FE39A, 0x3FE0F0, 0xFF6B6B, 0xE08BFF, 0xFFD166, 0xFFFFFF,
};
uint32_t vga_palette(int i) { return palette[i & 15]; }
int vga_font_w(void) { return fw; }
int vga_font_h(void) { return fh; }
int vga_font_bpr(void) { return fbpr; }
const uint8_t *vga_glyph(uint8_t c) { return glyphs + (uint32_t)c * fh * fbpr; }
int vga_cols(void) { return cols; }
int vga_rows(void) { return rows; }
bool vga_is_graphics(void) { return graphics; }

static void draw_cell(int x, int y, bool cursor) {
    uint16_t v = cells[y * cols + x];
    if (!graphics) { TEXT[y * 80 + x] = v; return; }
    uint16_t *sh = &shown[y * cols + x];
    if (!cursor && *sh == v) return;             /* unchanged: skip */
    *sh = cursor ? STALE : v;
    uint32_t fg = palette[(v >> 8) & 15], bg = palette[(v >> 12) & 15];
    const uint8_t *g = vga_glyph((uint8_t)v);
    uint32_t line[32];
    for (int r = 0; r < fh; r++, g += fbpr) {
        uint32_t bits = fbpr == 1 ? (uint32_t)g[0] << 8 : ((uint32_t)g[0] << 8) | g[1];
        bool cur = cursor && r >= fh - 3 && r < fh - 1;
        for (int c = 0; c < fw; c++) line[c] = (cur || (bits & (0x8000u >> c))) ? fg : bg;
        fb_write_row(ox + x * fw, oy + y * fh + r, line, fw);
    }
}

static void cursor_update(bool show) {
    if (hold) return;
    if (!graphics) {
        uint16_t pos = row * 80 + col;
        outb(0x3D4, 14); outb(0x3D5, pos >> 8);
        outb(0x3D4, 15); outb(0x3D5, pos & 0xFF);
    } else if (row < rows && col < cols) draw_cell(col, row, show);
}

void vga_redraw(void) {
    if (graphics) {
        fb_fill(0, 0, fb.width, fb.height, palette[VGA_BLACK]);
        for (int i = 0; i < cols * rows; i++) shown[i] = STALE;
    }
    for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) draw_cell(x, y, false);
    cursor_update(true);
}
void vga_hold(bool on) { hold = on; if (!on) vga_redraw(); }

void vga_init(void) {
    const uint8_t *f = asset_font;
    if (fb.present && f[0] == 'V' && f[1] == 'K' && f[2] == 'F' && f[3] == 'N') {
        fw = f[4]; fh = f[5]; fbpr = f[6]; glyphs = f + 8;
        graphics = true;
        cols = fb.width / fw;  if (cols > MAXC) cols = MAXC;
        rows = fb.height / fh; if (rows > MAXR) rows = MAXR;
        ox = (fb.width - cols * fw) / 2; oy = (fb.height - rows * fh) / 2;
    }
    vga_clear();
}

void vga_set_color(uint8_t fg, uint8_t bg) { color = fg | (bg << 4); }
uint8_t vga_get_color(void) { return color; }

void vga_clear(void) {
    for (int i = 0; i < cols * rows; i++) cells[i] = (uint16_t)' ' | (color << 8);
    row = col = 0;
    if (!hold) vga_redraw();
}

static void scroll(void) {
    memmove(cells, cells + cols, (size_t)cols * (rows - 1) * 2);
    for (int i = cols * (rows - 1); i < cols * rows; i++) cells[i] = (uint16_t)' ' | (color << 8);
    row = rows - 1;
    if (hold) return;
    for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) draw_cell(x, y, false);
}

void vga_putc(char c) {
    cursor_update(false);
    if (c == '\n') { col = 0; row++; }
    else if (c == '\r') col = 0;
    else if (c == '\b') { if (col > 0) col--; }
    else if (c == '\t') { col = (col + 4) & ~3; }
    else {
        cells[row * cols + col] = (uint16_t)(uint8_t)c | (color << 8);
        if (!hold) draw_cell(col, row, false);
        col++;
    }
    if (col >= cols) { col = 0; row++; }
    if (row >= rows) scroll();
    cursor_update(true);
}

void vga_write_at(int x, int y, const char *s, uint8_t attr) {
    if (y < 0 || y >= rows) return;
    for (; *s && x < cols; s++, x++) {
        if (x < 0) continue;
        cells[y * cols + x] = (uint16_t)(uint8_t)*s | (attr << 8);
        if (!hold) draw_cell(x, y, y == row && x == col);
    }
}
