/* VUKANG OS - drivers/keyboard.c : PS/2 keyboard (IRQ1), US layout, ring buffer */
#include "drivers.h"
#include "arch.h"
#include "io.h"

#define KBUF 256
static volatile char buf[KBUF];
static volatile uint32_t head, tail;
static int shift, caps, ctrl;

static const char map[128] = {
  0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
  'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
  'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
  'z','x','c','v','b','n','m',',','.','/', 0,'*', 0,' ' };
static const char map_shift[128] = {
  0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
  'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
  'A','S','D','F','G','H','J','K','L',':','"','~', 0,'|',
  'Z','X','C','V','B','N','M','<','>','?', 0,'*', 0,' ' };

static void push(char c) {
    uint32_t n = (head + 1) % KBUF;
    if (n != tail) { buf[head] = c; head = n; }
}
static uint32_t kbd_irq(regs_t *r) {
    uint8_t sc = inb(0x60);
    if (sc == 0x2A || sc == 0x36) shift = 1;
    else if (sc == 0xAA || sc == 0xB6) shift = 0;
    else if (sc == 0x1D) ctrl = 1;
    else if (sc == 0x9D) ctrl = 0;
    else if (sc == 0x3A) caps = !caps;
    else if (!(sc & 0x80) && sc < 128) {
        char c = shift ? map_shift[sc] : map[sc];
        if (caps && c >= 'a' && c <= 'z') c -= 32;
        else if (caps && c >= 'A' && c <= 'Z' && shift) c += 32;
        if (ctrl && c >= 'a' && c <= 'z') c = c - 'a' + 1;
        if (c) push(c);
    }
    return (uint32_t)r;
}
void keyboard_init(void) { irq_register(1, kbd_irq); }
int keyboard_getc_nonblock(void) {
    if (head == tail) return -1;
    char c = buf[tail]; tail = (tail + 1) % KBUF; return (uint8_t)c;
}
