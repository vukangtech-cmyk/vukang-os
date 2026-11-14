#ifndef VK_DRIVERS_H
#define VK_DRIVERS_H
#include <stdint.h>
#include <stdbool.h>
/* Keyboard */
void keyboard_init(void);
int  keyboard_getc_nonblock(void);   /* -1 if empty */
/* RTC */
typedef struct { int year, month, day, hour, minute, second; } rtc_time_t;
void rtc_read(rtc_time_t *t);
/* ATA PIO */
bool ata_init(void);
bool ata_present(void);
uint32_t ata_sectors(void);
const char *ata_model(void);
bool ata_read(uint32_t lba, uint8_t count, void *buf);
bool ata_write(uint32_t lba, uint8_t count, const void *buf);
/* Power */
void power_reboot(void);
void power_shutdown(void);
/* Speaker */
void pcspk_beep(uint32_t freq, uint32_t ms);
#endif
