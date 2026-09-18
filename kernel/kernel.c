/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 0 – Foundations)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. Right now it:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Prints a splash screen
 *     4. Runs a minimal interactive shell ("ksh")
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  – Process Management  →  process.h / process.c / scheduler.c
 *   Lecture 10  – Threads             →  thread.h  / thread.c
 *   Lecture 11  – Memory Management   →  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  – File System         →  fs.h      / fs.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc – use the PMM you build in Lecture 11
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "../include/types.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "process.h"
#include "scheduler.h"
#include "thread.h"
#include "mutex.h"
#include "pmm.h"
#include "fs.h"

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void race_thread_a(void);
static void race_thread_b(void);
static void racem_thread_a(void);
static void racem_thread_b(void);

/* Stage 2: race condition demo state */
static volatile int myglobal  = 0;
static volatile int race_done = 0;
static mutex_t       race_mutex;
#define RACE_ITERS 100000

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 0: Kernel Foundations", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering – Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath – only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  – PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      – kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   – physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  help    – Show this help message\n");
    vga_puts("  clear   – Clear the screen\n");
    vga_puts("  about   – About this OS and course\n");
    vga_puts("  echo    – Echo text to screen\n");
    vga_puts("  mem     – Memory map (stub)\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ps      – [L09] List processes\n");
    vga_puts("  kill    – [L09] Terminate a process\n");
    vga_puts("  race    – [L10] Unsynchronised myglobal++ race demo\n");
    vga_puts("  racem   – [L10] Same race, protected by a mutex\n");
    vga_puts("  meminfo – [L11] Show free/used physical memory\n");
    vga_puts("  ls      – [L12] List files\n");
    vga_puts("  cat     – [L12] Print file contents\n\n");
    vga_puts("  write   – [L12] Write text to a file\n");
    vga_puts("  rm      – [L12] Remove a file\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 – Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    /* Stage 0 stub – students implement the real PMM in Lecture 11 */
    vga_puts_color("\n  Memory Map (stub – implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  0x00000000 – 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 – 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0x00F00000 – 0x00FFFFFF  :  BIOS / ROM area\n");
    vga_puts("  0xB8000    – 0xBFFFF     :  VGA frame buffer\n");
    vga_puts_color("\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
                   VGA_YELLOW, VGA_BLACK);
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

/* Stage 4 filesystem shell commands */
static void cmd_fs_ls(void);
static void cmd_fs_touch(const char *name);
static void cmd_fs_cat(const char *name);
static void cmd_fs_write(const char *args);
static void cmd_fs_rm(const char *name);

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch */
        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")   == 0) { cmd_mem();   continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

                /* Stage 1: ps — list process table */
        if (k_strcmp(cmd, "ps") == 0) {
            vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
            vga_puts("  PID  STATE       NAME\n");
            vga_puts("  ---  ----------  --------\n");
            vga_set_color(VGA_WHITE, VGA_BLACK);
            const char *state_names[] = {
                "FREE", "READY", "RUNNING", "BLOCKED", "TERMINATED"
            };
            for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
                if (process_table[i].state != PROCESS_FREE) {
                    vga_printf("  %d    %-10s  %s\n",
                        process_table[i].pid,
                        state_names[process_table[i].state],
                        process_table[i].name);
                }
            }
            continue;
        }

        /* Stage 1: kill <pid> — terminate a process */
        if (k_strncmp(cmd, "kill ", 5) == 0) {
            const char *arg = cmd + 5;
            uint32_t pid = 0;
            while (*arg >= '0' && *arg <= '9') {
                pid = pid * 10 + (uint32_t)(*arg - '0');
                arg++;
            }
            if (pid == 0) {
                vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
                vga_puts("  Cannot kill kernel process (PID 0)\n");
            } else if (pid >= MAX_PROCESSES ||
                       process_table[pid].state == PROCESS_FREE) {
                vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
                vga_puts("  No such process\n");
            } else {
                process_terminate(pid);
                vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
                vga_printf("  Process %d terminated\n", pid);
            }
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            continue;
        }

        /* Stage 2: race — unsynchronised myglobal++ race condition demo */
        if (k_strcmp(cmd, "race") == 0) {
            myglobal  = 0;
            race_done = 0;
            thread_create("race_a", race_thread_a);
            thread_create("race_b", race_thread_b);
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("  Started race_a/race_b — no mutex yet.\n");
            vga_puts("  Expected final myglobal = 200000. Run 'ps' to watch,\n");
            vga_puts("  or wait for both '... done' lines to print.\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            continue;
        }

        /* Stage 2: racem — same race, protected by a mutex this time */
        if (k_strcmp(cmd, "racem") == 0) {
            myglobal  = 0;
            race_done = 0;
            mutex_init(&race_mutex);
            thread_create("racem_a", racem_thread_a);
            thread_create("racem_b", racem_thread_b);
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("  Started racem_a/racem_b — protected by a mutex.\n");
            vga_puts("  Expect exactly myglobal = 200000, every single run.\n");
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            continue;
        }

        /* Stage 3: meminfo — physical frame allocator usage */
        if (k_strcmp(cmd, "meminfo") == 0) {
            uint32_t total = pmm_total_frames();
            uint32_t freef  = pmm_free_frames();
            uint32_t used  = total - freef;
            vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
            vga_printf("  Free:  %u KB\n", freef * 4);
            vga_printf("  Used:  %u KB\n", used * 4);
            vga_printf("  Total: %u KB\n", total * 4);
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            continue;
        }

        /* Stage 4: RAM-disk filesystem */

if (k_strcmp(cmd, "ls") == 0) {

    cmd_fs_ls();

    continue;
}


if (k_strncmp(cmd, "touch ", 6) == 0) {

    cmd_fs_touch(
        k_ltrim(cmd + 6)
    );

    continue;
}


if (k_strncmp(cmd, "cat ", 4) == 0) {

    cmd_fs_cat(
        k_ltrim(cmd + 4)
    );

    continue;
}


if (k_strncmp(cmd, "write ", 6) == 0) {

    cmd_fs_write(
        k_ltrim(cmd + 6)
    );

    continue;
}


if (k_strncmp(cmd, "rm ", 3) == 0) {

    cmd_fs_rm(
        k_ltrim(cmd + 3)
    );

    continue;
}


/*
 * Alias:
 *
 * unlink filename
 */

if (k_strncmp(cmd, "unlink ", 7) == 0) {

    cmd_fs_rm(
        k_ltrim(cmd + 7)
    );

    continue;
}

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}




/* ---------------------------------------------------------------------------
 * Stage 4 shell helpers
 * --------------------------------------------------------------------------*/


static void cmd_fs_ls(void)
{
    fs_list();
}


/* ---------------------------------------------------------------------------
 * touch
 * --------------------------------------------------------------------------*/

static void cmd_fs_touch(const char *name)
{
    if (!name || !name[0]) {

        vga_puts(
            "  Usage: touch <name>\n"
        );

        return;
    }


    int32_t r =
        fs_create(name);


    if (r == -2) {

        vga_puts_color(
            "  File already exists.\n",
            VGA_YELLOW,
            VGA_BLACK
        );

    } else if (r < 0) {

        vga_puts_color(
            "  Could not create file.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );

    } else {

        vga_puts_color(
            "  File created.\n",
            VGA_LIGHT_GREEN,
            VGA_BLACK
        );
    }
}


/* ---------------------------------------------------------------------------
 * cat
 * --------------------------------------------------------------------------*/

static void cmd_fs_cat(const char *name)
{
    char buf[256];


    if (!name || !name[0]) {

        vga_puts(
            "  Usage: cat <name>\n"
        );

        return;
    }


    int32_t fd =
        fs_open(name);


    if (fd < 0) {

        vga_puts_color(
            "  File not found.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );

        return;
    }


    int32_t n;


    vga_puts("  ");


    while (
        (n = fs_read(
            fd,
            buf,
            sizeof(buf) - 1
        )) > 0
    ) {

        buf[n] = '\0';

        vga_puts(buf);
    }


    vga_puts("\n");


    fs_close(fd);
}


/* ---------------------------------------------------------------------------
 * write
 *
 * Usage:
 *
 *     write filename some text
 *
 * The shell command replaces the old file contents.
 * --------------------------------------------------------------------------*/

static void cmd_fs_write(const char *args)
{
    char name[FS_MAX_NAME];

    const char *p =
        args;

    uint32_t n = 0;


    if (!args || !args[0]) {

        vga_puts(
            "  Usage: write <name> <text>\n"
        );

        return;
    }


    /*
     * Skip spaces.
     */

    while (*p == ' ') {
        p++;
    }


    /*
     * Read filename.
     */

    while (
        *p &&
        *p != ' ' &&
        n + 1 < sizeof(name)
    ) {

        name[n++] =
            *p++;
    }


    name[n] =
        '\0';


    /*
     * Skip spaces between filename
     * and file contents.
     */

    while (*p == ' ') {
        p++;
    }


    if (!name[0] || !*p) {

        vga_puts(
            "  Usage: write <name> <text>\n"
        );

        return;
    }


    /*
     * If file already exists, remove it.
     *
     * This makes the shell command behave
     * like "replace file contents".
     */

    int32_t testfd =
        fs_open(name);


    if (testfd >= 0) {

        fs_close(testfd);

        fs_unlink(name);
    }


    /*
     * Create new file.
     */

    if (fs_create(name) < 0) {

        vga_puts_color(
            "  Could not create file.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );

        return;
    }


    /*
     * Open it.
     */

    int32_t fd =
        fs_open(name);


    if (fd < 0) {

        vga_puts_color(
            "  Could not open file.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );

        return;
    }


    /*
     * Write contents.
     */

    int32_t written =
        fs_write(
            fd,
            p,
            k_strlen(p)
        );


    fs_close(fd);


    if (written < 0) {

        vga_puts_color(
            "  Write failed.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );

    } else {

        vga_printf(
            "  Wrote %d bytes.\n",
            written
        );
    }
}


/* ---------------------------------------------------------------------------
 * rm / unlink
 * --------------------------------------------------------------------------*/

static void cmd_fs_rm(const char *name)
{
    if (!name || !name[0]) {

        vga_puts(
            "  Usage: rm <name>\n"
        );

        return;
    }


    int32_t r =
        fs_unlink(name);


    if (r == 0) {

        vga_puts_color(
            "  File removed.\n",
            VGA_LIGHT_GREEN,
            VGA_BLACK
        );

    } else {

        vga_puts_color(
            "  File not found or still open.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }
}



/* ---------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
/* Demo processes — visible proof that scheduling works */
static void task_a(void)
{
    for (int i = 0; i < 5; i++) {
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        vga_puts("[A] ");
        for (volatile uint32_t j = 0; j < 2000000; j++);
    }
    /* Done — terminate self */
    process_terminate(1);
    /* Spin until scheduler switches us away permanently */
    while (1) { __asm__ volatile("hlt"); }
}

static void task_b(void)
{
    for (int i = 0; i < 5; i++) {
        vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
        vga_puts("[B] ");
        for (volatile uint32_t j = 0; j < 2000000; j++);
    }
    /* Done — terminate self */
    process_terminate(2);
    while (1) { __asm__ volatile("hlt"); }
}

/* Stage 2: race condition demo — myglobal++ is LOAD -> ADD -> STORE,
 * not atomic. Run 'race' at the shell and watch the final value land
 * below 200000 most of the time. */
static void race_thread_a(void)
{
    for (int i = 0; i < RACE_ITERS; i++) myglobal++;
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_printf("\n  [race_a] done. myglobal = %d\n", myglobal);
    race_done++;
    thread_exit();
}

static void race_thread_b(void)
{
    for (int i = 0; i < RACE_ITERS; i++) myglobal++;
    vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
    vga_printf("\n  [race_b] done. myglobal = %d\n", myglobal);
    race_done++;
    thread_exit();
}

/* Stage 2: same race, this time with the increment protected by a
 * mutex — should land on exactly 200000 every time. */
static void racem_thread_a(void)
{
    for (int i = 0; i < RACE_ITERS; i++) {
        mutex_lock(&race_mutex);
        myglobal++;
        mutex_unlock(&race_mutex);
    }
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_printf("\n  [racem_a] done. myglobal = %d\n", myglobal);
    race_done++;
    thread_exit();
}

static void racem_thread_b(void)
{
    for (int i = 0; i < RACE_ITERS; i++) {
        mutex_lock(&race_mutex);
        myglobal++;
        mutex_unlock(&race_mutex);
    }
    vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
    vga_printf("\n  [racem_b] done. myglobal = %d\n", myglobal);
    race_done++;
    thread_exit();
}

void kernel_main(void) {
    vga_init();
    kb_init();

    /* Stage 1: interrupt infrastructure */
    idt_init();
    pic_init();
    pit_init();

    /* Register the timer ISR at vector 32 (IRQ0 after PIC remap).
     * Without this, idt_init() leaves entry 32 "not present" and
     * the PIT firing IRQ0 faults instead of running the scheduler. */
    extern void irq0_handler(void);
    idt_set_gate(32, (uint32_t)irq0_handler, 0x08, 0x8E);

    /* Stage 3: physical memory manager — parses the E820 map boot.asm
     * left at 0x8000/0x8004 and builds the frame bitmap. Must run
     * before anything tries to allocate a frame. */
    pmm_init();

    /* Stage 4: initialise RAM-disk filesystem */
    fs_init();

    /* Stage 1: process table and scheduler */
    process_init();
    scheduler_init();

    /* Create two demo processes */
    process_create("task_a", task_a);
    process_create("task_b", task_b);

    print_splash();

    /* Enable interrupts — timer starts firing NOW */
    __asm__ volatile("sti");

    /* Kernel thread (PID 0) continues as the shell */
    shell_run();

    /* Should never reach here */
    __asm__ __volatile__("hlt");
}