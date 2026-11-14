/* VUKANG OS - fs/blockdev.c : block device layer
 * Uses the ATA hard disk if one is attached, otherwise a 4 MiB RAM disk,
 * so the filesystem and database always have somewhere to live. */
#include "fs.h"
#include "drivers.h"
#include "mm.h"
#include "string.h"

#define RAMDISK_SECTORS 8192
static uint8_t *ramdisk;
static blockdev_t dev;

static bool ram_read(uint32_t lba, uint8_t count, void *buf) {
    uint32_t n = count ? count : 256;
    if (lba + n > RAMDISK_SECTORS) return false;
    memcpy(buf, ramdisk + lba * 512, n * 512); return true;
}
static bool ram_write(uint32_t lba, uint8_t count, const void *buf) {
    uint32_t n = count ? count : 256;
    if (lba + n > RAMDISK_SECTORS) return false;
    memcpy(ramdisk + lba * 512, buf, n * 512); return true;
}

void blockdev_init(void) {
    if (ata_init()) {
        dev.name = "ata0 (hard disk)"; dev.sectors = ata_sectors();
        dev.read = ata_read; dev.write = ata_write;
    } else {
        ramdisk = kzalloc(RAMDISK_SECTORS * 512);
        dev.name = "ram0 (RAM disk, not persistent)"; dev.sectors = RAMDISK_SECTORS;
        dev.read = ram_read; dev.write = ram_write;
    }
}
blockdev_t *blockdev_get(void) { return &dev; }
