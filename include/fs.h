#ifndef FS_H
#define FS_H

#include "../include/types.h"

/*
 * Stage 4 RAM Disk
 *
 * Total RAM disk:
 *     1 MiB
 *
 * Block size:
 *     4 KiB
 *
 * Number of blocks:
 *     256
 */

#define FS_BLOCK_SIZE 4096
#define FS_DISK_SIZE  (1024 * 1024)
#define FS_NUM_BLOCKS (FS_DISK_SIZE / FS_BLOCK_SIZE)

#define FS_MAX_INODES 64
#define FS_MAX_FDS    32

#define FS_MAX_NAME   24
#define FS_DIRECT     8

#define FS_ROOT_INODE 0


/*
 * Superblock
 *
 * Describes the whole filesystem.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_count;

    uint32_t inode_bitmap_block;
    uint32_t block_bitmap_block;

    uint32_t inode_table_start;
    uint32_t data_start;
} superblock_t;


/*
 * Inode
 *
 * Each file has one inode.
 *
 * 8 direct block pointers × 4096 bytes
 * = 32768 bytes
 * = 32 KiB maximum file size.
 */
typedef struct __attribute__((packed)) {
    uint8_t  used;
    uint8_t  type;
    uint16_t mode;

    uint32_t size;

    uint32_t blocks[FS_DIRECT];

    char name[FS_MAX_NAME];
} inode_t;


/*
 * Directory entry
 *
 * The Stage 4 filesystem uses a flat root directory.
 */
typedef struct __attribute__((packed)) {
    uint16_t inode;
    uint8_t  used;
    char     name[29];
} dirent_t;


/*
 * Open file descriptor.
 */
typedef struct {
    uint8_t  used;
    uint8_t  flags;
    uint16_t inode;

    uint32_t offset;
} file_t;


/* Filesystem initialisation */
void fs_init(void);


/* File creation */
int32_t fs_create(const char *name);


/* File operations */
int32_t fs_open(const char *name);

int32_t fs_read(
    int32_t fd,
    void *buf,
    uint32_t count
);

int32_t fs_write(
    int32_t fd,
    const void *buf,
    uint32_t count
);

int32_t fs_close(int32_t fd);


/* Remove a file */
int32_t fs_unlink(const char *name);


/* Directory listing */
void fs_list(void);


/* Get current file size */
uint32_t fs_size(int32_t fd);

#endif