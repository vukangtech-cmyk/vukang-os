#ifndef VK_FS_H
#define VK_FS_H
#include <stdint.h>
#include <stdbool.h>
/* Block device abstraction (ATA disk or RAM disk) */
typedef struct blockdev {
    const char *name;
    uint32_t sectors;
    bool (*read)(uint32_t lba, uint8_t count, void *buf);
    bool (*write)(uint32_t lba, uint8_t count, const void *buf);
} blockdev_t;
blockdev_t *blockdev_get(void);
void        blockdev_init(void);

/* VKFS - VUKANG File System */
#define VKFS_MAX_FILES 64
#define VKFS_NAME_LEN  40
typedef struct {
    char     name[VKFS_NAME_LEN];
    uint32_t size;
    uint32_t start;      /* first data sector */
    uint32_t alloc;      /* sectors allocated */
    uint32_t mtime;      /* packed date/time */
    uint32_t flags;      /* bit0 = used */
    uint32_t reserved;
} __attribute__((packed)) vkfs_entry_t;

bool vkfs_mount(void);
bool vkfs_format(void);
bool vkfs_mounted(void);
int  vkfs_read(const char *name, void *buf, uint32_t max);
bool vkfs_write(const char *name, const void *buf, uint32_t len);
bool vkfs_delete(const char *name);
bool vkfs_exists(const char *name);
int  vkfs_size(const char *name);
int  vkfs_list(vkfs_entry_t *out, int max);
void vkfs_info(uint32_t *total_sectors, uint32_t *free_sectors, uint32_t *files);
#endif
