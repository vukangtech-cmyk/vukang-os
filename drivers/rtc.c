/* VUKANG OS - drivers/rtc.c : CMOS real-time clock */
#include "drivers.h"
#include "io.h"
static uint8_t cmos(uint8_t reg) { outb(0x70, reg); return inb(0x71); }
static int bcd(int v) { return (v & 0x0F) + ((v >> 4) * 10); }
void rtc_read(rtc_time_t *t) {
    while (cmos(0x0A) & 0x80);           /* wait while update in progress */
    uint8_t s = cmos(0x00), m = cmos(0x02), h = cmos(0x04);
    uint8_t d = cmos(0x07), mo = cmos(0x08), y = cmos(0x09), b = cmos(0x0B);
    if (!(b & 0x04)) { s = bcd(s); m = bcd(m); h = bcd(h & 0x7F) | (h & 0x80); d = bcd(d); mo = bcd(mo); y = bcd(y); }
    t->second = s; t->minute = m; t->hour = h & 0x7F; t->day = d; t->month = mo; t->year = 2000 + y;
}
