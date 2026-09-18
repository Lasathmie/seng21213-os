# SENG21213-OS — Multi-Stage x86 Operating System

> **Course**: SENG 21213 – Computer Architecture & Operating Systems  
> **Year**: 2nd Year, Software Engineering  
> **Assignment**: Build your own x86 Operating System

---

## What Is This?

This project started from the Stage 0 kernel supplied for the semester-long OS
assignment. The kernel was extended through Stages 0–4 to implement process
management, scheduling, threads and synchronization, physical memory management,
and a RAM-disk file system.

```
seng21213-os/
├── boot/
│   └── boot.asm          ← MBR Bootloader (NASM, 16-bit → 32-bit transition)
├── kernel/
│   ├── kernel_entry.asm  ← Protected-mode entry, calls kernel_main()
│   ├── kernel.c          ← Main kernel: shell loop, command dispatch & stage initialization
│   ├── vga.c / vga.h     ← VGA 80×25 text-mode driver
│   ├── keyboard.c / .h   ← PS/2 keyboard polling driver
│   ├── process.c / .h    ← Process table, creation and termination
│   ├── scheduler.c / .h  ← Process scheduling
│   ├── thread.c / .h     ← Kernel thread support
│   ├── mutex.c / .h      ← Mutex synchronization
│   ├── pmm.c / .h        ← Physical memory management
│   └── fs.c / fs.h       ← RAM-disk file system
├── include/
│   └── types.h           ← Primitive types (no libc!)
├── linker.ld             ← Linker script (kernel at 0x10000)
├── Makefile              ← Build system
├── Dockerfile            ← Reproducible build environment
└── README.md             ← You are here
```

---

## Milestone Schedule

| Lecture | Milestone | Files to Add |
|---------|-----------|-------------|
| L08 | ✅ Stage 0 – Boot + VGA + Shell | *Given to you* |
| L09 | Process Management | `kernel/process.c`, `kernel/scheduler.c` |
| L10 | Threads & Synchronisation | `kernel/thread.c`, `kernel/mutex.c` |
| L11 | Memory Management | `kernel/pmm.c`, `kernel/vmm.c` |
| L12 | File System | `kernel/fs.c`, `kernel/ramdisk.c` |

---

## Quick Start

### Option A: Docker (Recommended for all platforms)

```bash
# 1. Install Docker Desktop (Windows/Mac) or Docker Engine (Linux)
# 2. Build the image once:
docker build -t seng21213-os-builder .

# 3. Build the OS:
docker run --rm -v "$(pwd)":/os seng21213-os-builder

# 4. Run in QEMU (install QEMU locally):
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M
```

### Option B: Native Linux/WSL2

```bash
# Ubuntu/Debian
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 make

# Build
make all

# Run
make run
```

### Option C: macOS (Homebrew)

```bash
brew install nasm x86_64-elf-binutils qemu

# You also need an i686-elf-gcc cross-compiler:
# See: https://wiki.osdev.org/GCC_Cross-Compiler
make all
make run
```

---

## Understanding the Boot Process

```
Power On
  │
  ▼
BIOS (firmware in ROM)
  │  Loads 512-byte MBR from disk sector 1 into RAM at 0x7C00
  ▼
boot/boot.asm  (Real Mode, 16-bit)
  │  Prints "Loading SENG21213-OS..."
  │  Reads 64 sectors (kernel) from disk into RAM at 0x10000
  │  Sets up GDT (Global Descriptor Table)
  │  Switches CPU to 32-bit Protected Mode
  │  Far-jumps to 0x10000
  ▼
kernel/kernel_entry.asm  (Protected Mode, 32-bit)
  │  Calls kernel_main()
  ▼
kernel/kernel.c  →  kernel_main()
  │  vga_init()     – set up text display
  │  kb_init()      – set up keyboard
  │  print_splash() – welcome screen
  │  shell_run()    – interactive shell (infinite loop)
  ▼
Stage 0–4 kernel implementation
  │
  ├─ Stage 1: processes + scheduler
  ├─ Stage 2: threads + mutex synchronization
  ├─ Stage 3: physical memory manager
  └─ Stage 4: RAM-disk file system
```

---

## Implemented Stages

### Stage 0 – Boot, VGA, Keyboard and Shell

The original Stage 0 foundation was retained and used as the base of the project.

Implemented/retained functionality includes:

  x86 boot process and protected-mode transition

  VGA text-mode display

  PS/2 keyboard input

  Kernel splash/about/help functionality

  Shell commands including clear and echo

  Interactive shell loop

  The original memory command remains a stub

### Stage 1 – Process Management and Scheduling

Stage 1 was added to the kernel and integrated into kernel_main().

Implemented functionality includes:

  Process table with a maximum of 16 process slots

  Process states and PCB management

  PID 0 kernel process initialization

  Process creation with per-process 4 KiB stacks

  Initial process context/frame construction

  Process termination for non-kernel processes

  Scheduler initialization

  Demo processes task_a and task_b

  ps shell command for displaying process information

  kill shell command for terminating a process

  Low-level context-switch support using PUSHAD/POPAD in the assembly context-switch path

### Stage 2 – Threads and Synchronisation

Stage 2 functionality was added through kernel threads and synchronization primitives.

Implemented functionality includes:

  Kernel thread support

  Mutex synchronization

  Race-condition demonstration code

  Mutex-based protection for the race demonstration

  Thread-related kernel initialization and integration

### Stage 3 – Physical Memory Management

Stage 3 added a physical memory manager to the kernel.

Implemented functionality includes:

  Physical memory manager (pmm.c)

  Physical memory allocation/free management

  Integration of the memory manager into the kernel build and initialization

### Stage 4 – RAM-Disk File System

Stage 4 added a RAM-disk file system and shell commands for basic file operations.

Implemented functionality includes:

  RAM-disk file system initialization through fs_init()

  Volatile RAM-based storage

  Superblock and bitmap-based file-system structures

  Inode table and flat root directory

  File descriptor management

  File creation, reading, writing and deletion operations

  Shell commands:

    ls

    touch

    cat

    write

    rm

  Integration of the file system into kernel_main()

The final kernel therefore combines the original Stage 0 foundation with the
implemented Stage 1 process management, Stage 2 threading/synchronisation,
Stage 3 physical memory management, and Stage 4 RAM-disk file system.


---

## Debugging Tips

```bash
# Debug with GDB
make run-debug
# In another terminal:
gdb
(gdb) target remote :1234
(gdb) set architecture i386
(gdb) symbol-file build/kernel.elf
(gdb) break kernel_main
(gdb) continue

# Inspect the disk image
xxd seng21213-os.img | head -32    # View MBR
xxd seng21213-os.img | grep -c aa55  # Verify boot signature
```

---

## Key Learning Resources

| Topic | Reference |
|-------|-----------|
| x86 Protected Mode | Intel IA-32 Manual, Vol 3, Chapter 3 |
| VGA Text Mode | OSDev Wiki: Text UI |
| Interrupts / IDT | Stallings Ch.1; OSDev: IDT |
| Process Management | Stallings Ch.3–4 (your lecture notes) |
| Memory Management | Stallings Ch.7–8 (your lecture notes) |
| OSDev community | https://wiki.osdev.org |

---

## Assessment Rubric (per milestone)

| Criterion | Weight |
|-----------|--------|
| Code compiles and kernel boots in QEMU | 30% |
| Feature implementation (correct behaviour) | 40% |
| Code quality and comments | 20% |
| Lab demo and viva questions | 10% |

---

*Happy hacking! Remember: every commercial OS started exactly like this.*
