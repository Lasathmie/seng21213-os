#include "fs.h"
#include "vga.h"


/*
 * ============================================================================
 * SENG21213-OS :: Stage 4 RAM Disk File System
 * ============================================================================
 *
 * RAM disk size:
 *     1 MiB
 *
 * Block size:
 *     4 KiB
 *
 * Total blocks:
 *     256
 *
 * Disk layout:
 *
 *     Block 0       Superblock
 *     Block 1       Inode bitmap
 *     Block 2       Block bitmap
 *     Block 3       Inode table
 *     Block 4       Root directory
 *     Block 5+      File data
 *
 * Each inode contains 8 direct block pointers.
 *
 * Therefore:
 *
 *     8 × 4096 = 32768 bytes
 *
 * Maximum Stage 4 file size = 32 KiB.
 *
 * A single-indirect pointer can be added later as an extension.
 * ============================================================================
 */


/* ---------------------------------------------------------------------------
 * Filesystem constants
 * --------------------------------------------------------------------------*/

#define FS_MAGIC       0x53454E47u

#define FS_FILE        1
#define FS_DIR         2

#define FS_INODE_TABLE 3
#define FS_ROOT_BLOCK  4
#define FS_DATA_START  5


/* ---------------------------------------------------------------------------
 * RAM disk
 *
 * This is the actual filesystem storage.
 *
 * It lives completely in RAM.
 * --------------------------------------------------------------------------*/

#define RAMDISK_ADDR 0x00200000
static uint8_t *ramdisk = (uint8_t *)RAMDISK_ADDR;


/* ---------------------------------------------------------------------------
 * Filesystem structures
 * --------------------------------------------------------------------------*/

static superblock_t *sb;

static inode_t *inodes;

static uint8_t *inode_bitmap;

static uint8_t *block_bitmap;

static dirent_t *root_dir;


/* ---------------------------------------------------------------------------
 * Open file table
 * --------------------------------------------------------------------------*/

static file_t fds[FS_MAX_FDS];


/* ===========================================================================
 * Basic memory/string functions
 * ===========================================================================*/


static void k_memset(
    void *dst,
    uint8_t value,
    uint32_t n
)
{
    uint8_t *d = (uint8_t *)dst;

    while (n--) {
        *d++ = value;
    }
}


static void k_memcpy(
    void *dst,
    const void *src,
    uint32_t n
)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    while (n--) {
        *d++ = *s++;
    }
}


static uint32_t k_strlen(const char *s)
{
    uint32_t n = 0;

    while (s[n]) {
        n++;
    }

    return n;
}


static int k_strcmp(
    const char *a,
    const char *b
)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return (uint8_t)*a - (uint8_t)*b;
}


static void k_strncpy(
    char *dst,
    const char *src,
    uint32_t max
)
{
    uint32_t i = 0;

    if (max == 0) {
        return;
    }

    while (i + 1 < max && src[i]) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}


/* ===========================================================================
 * Bitmap functions
 * ===========================================================================*/


static int bit_test(
    const uint8_t *map,
    uint32_t n
)
{
    return (map[n / 8] >> (n % 8)) & 1;
}


static void bit_set(
    uint8_t *map,
    uint32_t n
)
{
    map[n / 8] |=
        (uint8_t)(1u << (n % 8));
}


static void bit_clear(
    uint8_t *map,
    uint32_t n
)
{
    map[n / 8] &=
        (uint8_t)~(1u << (n % 8));
}


/* ===========================================================================
 * Inode allocation
 * ===========================================================================*/


static int alloc_inode(void)
{
    /*
     * inode 0 is reserved for the root directory.
     */
    for (uint32_t i = 1;
         i < FS_MAX_INODES;
         i++) {

        if (!bit_test(inode_bitmap, i)) {

            bit_set(inode_bitmap, i);

            k_memset(
                &inodes[i],
                0,
                sizeof(inode_t)
            );

            inodes[i].used = 1;
            inodes[i].type = FS_FILE;

            return (int)i;
        }
    }

    return -1;
}


static void free_inode(uint32_t ino)
{
    /*
     * Never free the root inode.
     */
    if (ino == FS_ROOT_INODE ||
        ino >= FS_MAX_INODES) {

        return;
    }

    bit_clear(inode_bitmap, ino);

    k_memset(
        &inodes[ino],
        0,
        sizeof(inode_t)
    );
}


/* ===========================================================================
 * Data block allocation
 * ===========================================================================*/


static int alloc_block(void)
{
    /*
     * Blocks 0-4 are filesystem metadata.
     *
     * File data begins at block 5.
     */
    for (uint32_t b = FS_DATA_START;
         b < FS_NUM_BLOCKS;
         b++) {

        if (!bit_test(block_bitmap, b)) {

            bit_set(block_bitmap, b);

            /*
             * Clear newly allocated block.
             */
            k_memset(
                ramdisk + b * FS_BLOCK_SIZE,
                0,
                FS_BLOCK_SIZE
            );

            return (int)b;
        }
    }

    return -1;
}


static void free_block(uint32_t b)
{
    if (b < FS_DATA_START ||
        b >= FS_NUM_BLOCKS) {

        return;
    }

    bit_clear(block_bitmap, b);
}


/* ===========================================================================
 * Directory functions
 * ===========================================================================*/


static int find_dirent(const char *name)
{
    uint32_t entries =
        FS_BLOCK_SIZE / sizeof(dirent_t);

    for (uint32_t i = 0;
         i < entries;
         i++) {

        if (root_dir[i].used &&
            k_strcmp(root_dir[i].name, name) == 0) {

            return (int)i;
        }
    }

    return -1;
}


static int add_dirent(
    const char *name,
    uint32_t ino
)
{
    uint32_t entries =
        FS_BLOCK_SIZE / sizeof(dirent_t);

    for (uint32_t i = 0;
         i < entries;
         i++) {

        if (!root_dir[i].used) {

            root_dir[i].used = 1;

            root_dir[i].inode =
                (uint16_t)ino;

            k_strncpy(
                root_dir[i].name,
                name,
                sizeof(root_dir[i].name)
            );

            return 0;
        }
    }

    return -1;
}


static void remove_dirent(uint32_t index)
{
    uint32_t entries =
        FS_BLOCK_SIZE / sizeof(dirent_t);

    if (index < entries) {

        k_memset(
            &root_dir[index],
            0,
            sizeof(dirent_t)
        );
    }
}


/* ===========================================================================
 * Filesystem initialisation
 * ===========================================================================*/


void fs_init(void)
{
    /*
     * Our boot path does not explicitly zero the BSS.
     *
     * Therefore initialise the RAM disk ourselves.
     */
    k_memset(
        ramdisk,
        0,
        FS_DISK_SIZE
    );

    k_memset(
        fds,
        0,
        sizeof(fds)
    );


    /*
     * Set filesystem structure addresses.
     */

    sb =
        (superblock_t *)
        (ramdisk + 0 * FS_BLOCK_SIZE);

    inode_bitmap =
        ramdisk + 1 * FS_BLOCK_SIZE;

    block_bitmap =
        ramdisk + 2 * FS_BLOCK_SIZE;

    inodes =
        (inode_t *)
        (ramdisk + 3 * FS_BLOCK_SIZE);

    root_dir =
        (dirent_t *)
        (ramdisk + FS_ROOT_BLOCK *
         FS_BLOCK_SIZE);


    /*
     * Build superblock.
     */

    sb->magic =
        FS_MAGIC;

    sb->block_size =
        FS_BLOCK_SIZE;

    sb->total_blocks =
        FS_NUM_BLOCKS;

    sb->inode_count =
        FS_MAX_INODES;

    sb->inode_bitmap_block =
        1;

    sb->block_bitmap_block =
        2;

    sb->inode_table_start =
        3;

    sb->data_start =
        FS_DATA_START;


    /*
     * Reserve filesystem metadata blocks.
     *
     * Blocks:
     *
     * 0 = superblock
     * 1 = inode bitmap
     * 2 = block bitmap
     * 3 = inode table
     * 4 = root directory
     */

    for (uint32_t b = 0;
         b < FS_DATA_START;
         b++) {

        bit_set(
            block_bitmap,
            b
        );
    }


    /*
     * Create root inode.
     */

    bit_set(
        inode_bitmap,
        FS_ROOT_INODE
    );


    inodes[FS_ROOT_INODE].used =
        1;

    inodes[FS_ROOT_INODE].type =
        FS_DIR;

    inodes[FS_ROOT_INODE].blocks[0] =
        FS_ROOT_BLOCK;

    k_strncpy(
        inodes[FS_ROOT_INODE].name,
        "/",
        FS_MAX_NAME
    );


    vga_puts_color(
        "  [FS] RAM disk mounted: "
        "1 MiB, 256 x 4 KiB blocks\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );
}


/* ===========================================================================
 * Create
 * ===========================================================================*/


int32_t fs_create(const char *name)
{
    if (!name ||
        !name[0] ||
        k_strlen(name) >= FS_MAX_NAME) {

        return -1;
    }


    /*
     * Do not allow duplicate names.
     */

    if (find_dirent(name) >= 0) {
        return -2;
    }


    int ino =
        alloc_inode();

    if (ino < 0) {
        return -3;
    }


    /*
     * Add directory entry.
     */

    if (add_dirent(
            name,
            (uint32_t)ino) < 0) {

        free_inode(
            (uint32_t)ino
        );

        return -4;
    }


    k_strncpy(
        inodes[ino].name,
        name,
        FS_MAX_NAME
    );


    return ino;
}


/* ===========================================================================
 * Open
 * ===========================================================================*/


int32_t fs_open(const char *name)
{
    int d =
        find_dirent(name);

    if (d < 0) {
        return -1;
    }


    uint32_t ino =
        root_dir[d].inode;


    if (ino >= FS_MAX_INODES ||
        !inodes[ino].used) {

        return -1;
    }


    /*
     * Find free file descriptor.
     */

    for (int32_t fd = 0;
         fd < FS_MAX_FDS;
         fd++) {

        if (!fds[fd].used) {

            fds[fd].used =
                1;

            fds[fd].flags =
                0;

            fds[fd].inode =
                (uint16_t)ino;

            fds[fd].offset =
                0;

            return fd;
        }
    }


    return -2;
}


/* ===========================================================================
 * Read
 * ===========================================================================*/


int32_t fs_read(
    int32_t fd,
    void *buf,
    uint32_t count
)
{
    if (fd < 0 ||
        fd >= FS_MAX_FDS ||
        !fds[fd].used ||
        !buf) {

        return -1;
    }


    inode_t *ino =
        &inodes[fds[fd].inode];


    if (!ino->used ||
        ino->type != FS_FILE) {

        return -1;
    }


    /*
     * End of file.
     */

    if (fds[fd].offset >=
        ino->size) {

        return 0;
    }


    uint32_t available =
        ino->size -
        fds[fd].offset;


    if (count > available) {
        count = available;
    }


    uint8_t *out =
        (uint8_t *)buf;


    uint32_t done = 0;


    while (done < count) {

        uint32_t pos =
            fds[fd].offset;


        uint32_t block_index =
            pos / FS_BLOCK_SIZE;


        uint32_t block_off =
            pos % FS_BLOCK_SIZE;


        if (block_index >= FS_DIRECT ||
            ino->blocks[block_index] == 0) {

            break;
        }


        uint32_t n =
            FS_BLOCK_SIZE -
            block_off;


        if (n > count - done) {
            n = count - done;
        }


        k_memcpy(
            out + done,

            ramdisk +
            ino->blocks[block_index] *
            FS_BLOCK_SIZE +
            block_off,

            n
        );


        fds[fd].offset += n;

        done += n;
    }


    return (int32_t)done;
}


/* ===========================================================================
 * Write
 * ===========================================================================*/


int32_t fs_write(
    int32_t fd,
    const void *buf,
    uint32_t count
)
{
    if (fd < 0 ||
        fd >= FS_MAX_FDS ||
        !fds[fd].used ||
        !buf) {

        return -1;
    }


    inode_t *ino =
        &inodes[fds[fd].inode];


    if (!ino->used ||
        ino->type != FS_FILE) {

        return -1;
    }


    /*
     * Stage 4 supports 8 direct blocks.
     *
     * Maximum:
     *
     * 8 × 4096 = 32768 bytes
     */

    uint32_t max_bytes =
        FS_DIRECT * FS_BLOCK_SIZE;


    if (fds[fd].offset >
        max_bytes) {

        return -1;
    }


    uint32_t writable =
        max_bytes -
        fds[fd].offset;


    if (count > writable) {
        count = writable;
    }


    const uint8_t *src =
        (const uint8_t *)buf;


    uint32_t done = 0;


    while (done < count) {

        uint32_t pos =
            fds[fd].offset;


        uint32_t block_index =
            pos / FS_BLOCK_SIZE;


        uint32_t block_off =
            pos % FS_BLOCK_SIZE;


        if (block_index >= FS_DIRECT) {
            break;
        }


        /*
         * Allocate a data block if this
         * is the first write to it.
         */

        if (ino->blocks[block_index] == 0) {

            int b =
                alloc_block();


            if (b < 0) {
                break;
            }


            ino->blocks[block_index] =
                (uint32_t)b;
        }


        uint32_t n =
            FS_BLOCK_SIZE -
            block_off;


        if (n > count - done) {
            n = count - done;
        }


        k_memcpy(

            ramdisk +
            ino->blocks[block_index] *
            FS_BLOCK_SIZE +
            block_off,

            src + done,

            n
        );


        fds[fd].offset += n;

        done += n;
    }


    /*
     * Update file size.
     */

    if (fds[fd].offset >
        ino->size) {

        ino->size =
            fds[fd].offset;
    }


    return (int32_t)done;
}


/* ===========================================================================
 * Close
 * ===========================================================================*/


int32_t fs_close(int32_t fd)
{
    if (fd < 0 ||
        fd >= FS_MAX_FDS ||
        !fds[fd].used) {

        return -1;
    }


    k_memset(
        &fds[fd],
        0,
        sizeof(file_t)
    );


    return 0;
}


/* ===========================================================================
 * Unlink
 * ===========================================================================*/


int32_t fs_unlink(const char *name)
{
    int d =
        find_dirent(name);


    if (d < 0) {
        return -1;
    }


    uint32_t ino_num =
        root_dir[d].inode;


    /*
     * Never delete root.
     */

    if (ino_num == FS_ROOT_INODE ||
        ino_num >= FS_MAX_INODES) {

        return -1;
    }


    inode_t *ino =
        &inodes[ino_num];


    /*
     * Refuse deletion if the file
     * is currently open.
     */

    for (int32_t fd = 0;
         fd < FS_MAX_FDS;
         fd++) {

        if (fds[fd].used &&
            fds[fd].inode ==
                ino_num) {

            return -2;
        }
    }


    /*
     * Free all data blocks.
     */

    for (uint32_t i = 0;
         i < FS_DIRECT;
         i++) {

        if (ino->blocks[i]) {

            free_block(
                ino->blocks[i]
            );
        }
    }


    /*
     * Free inode.
     */

    free_inode(
        ino_num
    );


    /*
     * Remove directory entry.
     */

    remove_dirent(
        (uint32_t)d
    );


    return 0;
}


/* ===========================================================================
 * ls
 * ===========================================================================*/


void fs_list(void)
{
    vga_puts_color(
        "\n  Name                     Size\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );


    vga_puts(
        "  ------------------------ -----\n"
    );


    int count = 0;


    uint32_t entries =
        FS_BLOCK_SIZE /
        sizeof(dirent_t);


    for (uint32_t i = 0;
         i < entries;
         i++) {

        if (!root_dir[i].used) {
            continue;
        }


        uint32_t ino_num =
            root_dir[i].inode;


        if (ino_num >= FS_MAX_INODES ||
            !inodes[ino_num].used) {

            continue;
        }


        vga_printf(
            "  %-24s %u bytes\n",
            root_dir[i].name,
            inodes[ino_num].size
        );


        count++;
    }


    if (!count) {
        vga_puts(
            "  <empty>\n"
        );
    }
}


/* ===========================================================================
 * File size
 * ===========================================================================*/


uint32_t fs_size(int32_t fd)
{
    if (fd < 0 ||
        fd >= FS_MAX_FDS ||
        !fds[fd].used) {

        return 0;
    }


    return inodes[
        fds[fd].inode
    ].size;
}