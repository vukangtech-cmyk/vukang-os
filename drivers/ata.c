/* VUKANG OS - drivers/ata.c : ATA PIO (LBA28) driver, primary master */
#include "drivers.h"
#include "io.h"
#include "string.h"

#define ATA_DATA   0x1F0
#define ATA_ERR    0x1F1
#define ATA_COUNT  0x1F2
#define ATA_LBA0   0x1F3
#define ATA_LBA1   0x1F4
#define ATA_LBA2   0x1F5
#define ATA_DRIVE  0x1F6
#define ATA_CMD    0x1F7
#define ATA_CTRL   0x3F6
#define ST_BSY 0x80
#define ST_DRQ 0x08
#define ST_ERR 0x01
#define ST_DF  0x20

static bool present;
static uint32_t total;
static char model[41];

static bool wait_bsy(void) { for (int i = 0; i < 1000000; i++) if (!(inb(ATA_CMD) & ST_BSY)) return true; return false; }
static bool wait_drq(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(ATA_CMD);
        if (s & (ST_ERR | ST_DF)) return false;
        if (!(s & ST_BSY) && (s & ST_DRQ)) return true;
    }
    return false;
}

bool ata_init(void) {
    outb(ATA_CTRL, 0x02);                 /* disable ATA IRQs, we poll */
    outb(ATA_DRIVE, 0xA0);
    for (int i = 0; i < 4; i++) inb(ATA_CMD);
    outb(ATA_COUNT, 0); outb(ATA_LBA0, 0); outb(ATA_LBA1, 0); outb(ATA_LBA2, 0);
    outb(ATA_CMD, 0xEC);                  /* IDENTIFY */
    uint8_t st = inb(ATA_CMD);
    if (st == 0 || st == 0xFF) return present = false;
    if (!wait_bsy()) return present = false;
    if (inb(ATA_LBA1) || inb(ATA_LBA2)) return present = false;  /* ATAPI/SATA, not ATA */
    if (!wait_drq()) return present = false;
    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(ATA_DATA);
    total = id[60] | ((uint32_t)id[61] << 16);
    if (total > 0x0FFFFFFFu) total = 0x0FFFFFFFu;
    for (int i = 0; i < 20; i++) { model[i * 2] = id[27 + i] >> 8; model[i * 2 + 1] = id[27 + i] & 0xFF; }
    model[40] = 0;
    for (int i = 39; i >= 0 && model[i] == ' '; i--) model[i] = 0;
    return present = total > 0;
}
bool ata_present(void) { return present; }
uint32_t ata_sectors(void) { return total; }
const char *ata_model(void) { return model; }

static void setup(uint32_t lba, uint8_t count, uint8_t cmd) {
    wait_bsy();
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_COUNT, count);
    outb(ATA_LBA0, lba & 0xFF);
    outb(ATA_LBA1, (lba >> 8) & 0xFF);
    outb(ATA_LBA2, (lba >> 16) & 0xFF);
    outb(ATA_CMD, cmd);
}
static bool in_range(uint32_t lba, uint8_t count) {
    uint32_t n = count ? count : 256;
    return lba < total && n <= total - lba && lba + n <= 0x10000000u;   /* LBA28 limit */
}
bool ata_read(uint32_t lba, uint8_t count, void *buf) {
    if (!present || !buf || !in_range(lba, count)) return false;
    uint32_t f = irq_save();
    setup(lba, count, 0x20);
    uint16_t *p = buf;
    for (int s = 0; s < (count ? count : 256); s++) {
        if (!wait_drq()) { irq_restore(f); return false; }
        for (int i = 0; i < 256; i++) *p++ = inw(ATA_DATA);
    }
    irq_restore(f);
    return true;
}
bool ata_write(uint32_t lba, uint8_t count, const void *buf) {
    if (!present || !buf || !in_range(lba, count)) return false;
    uint32_t f = irq_save();
    setup(lba, count, 0x30);
    const uint16_t *p = buf;
    for (int s = 0; s < (count ? count : 256); s++) {
        if (!wait_drq()) { irq_restore(f); return false; }
        for (int i = 0; i < 256; i++) outw(ATA_DATA, *p++);
    }
    outb(ATA_CMD, 0xE7);                  /* CACHE FLUSH */
    wait_bsy();
    irq_restore(f);
    return true;
}
