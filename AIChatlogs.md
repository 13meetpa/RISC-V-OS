--Meetkumar Patel

I am using OpenAI's 20 usd for this assginment and here are the chats for the assgiments:

Chat 1:

Input: 

so how can I make a command line OS for a for a RISC-V computer and when the OS is done with it has be capable to run on a QEMU and for we use the virt Generic Platform. But it has to be running on ubuntu Linux where QEMU is not installed afaict.

Output:
You basically need to solve two problems in parallel:

Get a working RISC-V + QEMU environment on your Ubuntu machine.

Structure your “OS” so it can boot as a kernel on the QEMU virt board and give you a command line.

I’ll walk you through both, step-by-step, and then we can worry about “fancy features” later.

1. Set up the RISC-V + QEMU environment on Ubuntu

I’m going to assume:

You’re on Ubuntu (bare metal or VM).

You have sudo access.

1.1. Install QEMU with RISC-V support
sudo apt update
sudo apt install qemu-system-misc qemu-utils


On most modern Ubuntus, this gives you qemu-system-riscv64. Check:

qemu-system-riscv64 --version


If that command works, you’re good. If it doesn’t, tell me the exact error and we can adjust (worst case: build QEMU from source, but usually not needed).

1.2. Install a RISC-V cross compiler

You want something like riscv64-unknown-elf-gcc (bare-metal) or riscv64-linux-gnu-gcc (Linux-targeted). For a small teaching OS, bare-metal is usually easier:

sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf


Check:

riscv64-unknown-elf-gcc --version

2. Decide what “OS” means for this project

For your assignment, “operating system” doesn’t have to be Linux-level complicated. A reasonable target:

It boots on QEMU virt board.

Sets up basic machine state (stack, maybe paging or just flat mapping).

Prints to the console (UART).

Has a command-line loop where you type simple commands:

help – list commands

echo <text> – echo text

time – show a fake or simple counter

maybe run demo – “start” a second “program” (in reality, a function)

Then you can add “features” your prof listed:

Loading separate programs: Have multiple C functions and treat them as “programs”; you can simulate loading by jumping to them.

Multiple programs simultaneously: Implement super simple cooperative “threads” with a scheduler.

Synchronization / protection: Basic locks + maybe address separation (even if not full paging).

Don’t worry: we can layer these later. First: boot + print + simple shell.

3. Minimal OS layout (high level)

You’ll typically have a directory like:

riscv-os/
  Makefile
  linker.ld
  start.S        # low-level boot code (assembly)
  kernel.c       # main() / kmain() and shell
  uart.c/.h      # for printing / input over serial

3.1. Linker script (linker.ld)

You need to link your kernel to the right physical address QEMU expects for virt. A common simple choice is to link at 0x80000000:

ENTRY(_start)

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text.boot)
    *(.text*)
  }

  .rodata : {
    *(.rodata*)
  }

  .data : {
    *(.data*)
  }

  .bss : {
    *(.bss*)
    *(COMMON)
  }
}


This tells the linker that the _start symbol (your first instruction) will be at 0x80000000, which is where the virt board typically jumps for the kernel.

3.2. Boot assembly (start.S)

You need to:

Set up a stack.

Jump into C code (kmain).

Something like:

    .section .text.boot
    .globl _start
_start:
    # set up stack pointer (just pick some high address in RAM)
    la sp, stack_top

    # call C kernel main
    call kmain

    # if kmain returns, just loop
1:  j 1b

    .bss
    .align 16
stack:
    .space 4096 * 4     # 16KB stack
stack_top:

3.3. Simple UART driver (uart.c)

For the virt machine, the UART (serial console) is usually at physical address 0x10000000. A super simple polled UART driver:

// uart.h
#ifndef UART_H
#define UART_H

void uart_putc(char c);
void uart_puts(const char *s);
int  uart_getc_blocking(void);

#endif

// uart.c
#include <stdint.h>
#include "uart.h"

#define UART0_BASE 0x10000000UL
#define UART_THR   0x00  // transmit holding
#define UART_LSR   0x05  // line status
#define UART_LSR_THRE 0x20  // transmitter holding register empty
#define UART_RBR   0x00  // receiver buffer
#define UART_LSR_DR  0x01  // data ready

static inline uint8_t mmio_read8(uintptr_t addr) {
    return *(volatile uint8_t *)addr;
}

static inline void mmio_write8(uintptr_t addr, uint8_t val) {
    *(volatile uint8_t *)addr = val;
}

void uart_putc(char c) {
    // wait until THR empty
    while ((mmio_read8(UART0_BASE + UART_LSR) & UART_LSR_THRE) == 0)
        ;
    mmio_write8(UART0_BASE + UART_THR, c);
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

int uart_getc_blocking(void) {
    // wait until data ready
    while ((mmio_read8(UART0_BASE + UART_LSR) & UART_LSR_DR) == 0)
        ;
    return mmio_read8(UART0_BASE + UART_RBR);
}


This is enough for printf-style output (you can write your own tiny kprintf later) and reading keystrokes.

3.4. Kernel main + shell (kernel.c)
#include <stdint.h>
#include "uart.h"

static void read_line(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        int c = uart_getc_blocking();
        if (c == '\r' || c == '\n') {
            uart_putc('\r');
            uart_putc('\n');
            break;
        } else if (c == 0x7f || c == '\b') { // backspace
            if (i > 0) {
                i--;
                uart_puts("\b \b");
            }
        } else {
            buf[i++] = (char)c;
            uart_putc((char)c);
        }
    }
    buf[i] = '\0';
}

static int streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++; b++;
    }
    return *a == *b;
}

void kmain(void) {
    uart_puts("Simple RISC-V OS booted!\n");
    uart_puts("Type 'help' for commands.\n\n");

    char line[128];

    while (1) {
        uart_puts("> ");
        read_line(line, sizeof(line));

        if (streq(line, "help")) {
            uart_puts("Commands:\n");
            uart_puts("  help     - show this help\n");
            uart_puts("  echo ... - echo text\n");
            uart_puts("  reboot   - (not implemented, just message)\n");
        } else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' && line[3] == 'o' && (line[4] == ' ' || line[4] == '\0')) {
            uart_puts(line + 5);
            uart_puts("\n");
        } else if (streq(line, "reboot")) {
            uart_puts("Pretending to reboot... (not implemented)\n");
        } else if (line[0] == '\0') {
            // empty line: ignore
        } else {
            uart_puts("Unknown command: ");
            uart_puts(line);
            uart_puts("\n");
        }
    }
}


This gives you a basic command line OS. From here, you can extend with “multiple programs”, simple scheduler, etc.

4. Building it for RISC-V

Example Makefile:

CROSS   = riscv64-unknown-elf-
CC      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS  = -march=rv64imac -mabi=lp64 -ffreestanding -nostdlib -nostartfiles -O2
LDFLAGS = -T linker.ld -nostdlib

all: kernel.elf kernel.bin

kernel.elf: start.o kernel.o uart.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o kernel.elf kernel.bin


Then:

make


If all goes well, you’ll get kernel.elf and kernel.bin.

5. Running your OS on QEMU virt

For a bare-metal RV64 “kernel” on the virt board, a common command is:

qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios none \
  -kernel kernel.elf


-nographic – use your terminal as the serial console.

-bios none – QEMU will jump directly to your kernel entry (0x80000000).

-kernel kernel.elf – the ELF we built.

If everything is wired up correctly, you should see:

Simple RISC-V OS booted!
Type 'help' for commands.

>


Then you can type help, echo hello, etc.

6. How this ties into your class assignment

Your professor’s requirements:

Loading separate programs: add multiple “program” functions and a super simple loader/scheduler that can switch between them.

Running multiple programs simultaneously: implement a minimal scheduler (round-robin, time-slice via a timer interrupt, or cooperative yield()).

Synchronization: simple spinlocks around shared data structures.

File system?: you can fake a “filesystem” in RAM (array of “files”) to start.

How to create/load new programs: define a trivial executable format (struct with entry point, stack, etc.) and “load” by copying into a fixed region.

But none of that is possible until you have:

Toolchain & QEMU working.

A tiny kernel that boots and prints.

A basic shell.

So: step 1 for you right now is:

On Ubuntu:

sudo apt update
sudo apt install qemu-system-misc gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
qemu-system-riscv64 --version
riscv64-unknown-elf-gcc --version


Create a folder riscv-os/ with the 4 files:

linker.ld

start.S

uart.c / uart.h

kernel.c

Makefile

Run make, then run QEMU with the command above.

If you want, next message you can paste any errors from make or qemu-system-riscv64 and we’ll debug the environment, then start layering features like “multiple programs” and “synchronization.”

Chat 2: 

Input:
so for this project, this has to be build with many people we have to push the code to github this is a group project? Based on that can you help me setup the repo property so that more people with different OSs can run this project?

Output: 
Yep, this is exactly the kind of project where repo setup + tooling matters a lot, otherwise everyone’s machine turns into a special snowflake.

Let’s set things up so that:

✅ Everyone uses the same commands (make, make run)

✅ Toolchain/QEMU names can differ per machine (Linux, macOS, WSL, etc.)

✅ Build artifacts aren’t committed

✅ Setup instructions for each OS are clear

1. Recommended repo structure

Here’s a simple, group-friendly layout:

riscv-os/
├─ src/
│  ├─ start.S        # boot assembly (_start, stack, jump to kmain)
│  ├─ kernel.c       # OS "main" + simple shell
│  ├─ uart.c         # UART driver
│  └─ uart.h
├─ linker.ld         # linker script (load at 0x80000000)
├─ Makefile          # top-level, builds + runs QEMU
├─ .gitignore
├─ README.md
└─ docs/
   ├─ SETUP.md       # per-OS setup instructions
   └─ DESIGN.md      # OS design, notes, etc.


Everyone on all OSes will run:

make
make run


Everything else (toolchain name, QEMU path) is configurable via variables.

2. Cross-platform Makefile (core of the project)

Create a top-level Makefile like this:

# ==========
# Toolchain
# ==========
# Default: GCC bare-metal toolchain (change via environment)
CROSS_COMPILE ?= riscv64-unknown-elf-

CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS  = -march=rv64imac -mabi=lp64 -ffreestanding -nostdlib -nostartfiles -O2
LDFLAGS = -T linker.ld -nostdlib

# ==========
# QEMU
# ==========
QEMU         ?= qemu-system-riscv64
QEMU_MACHINE ?= virt

# ==========
# Files
# ==========
SRCS = src/kernel.c src/uart.c src/start.S
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

all: kernel.elf kernel.bin

kernel.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

# ==========
# Run in QEMU
# ==========
run: kernel.elf
	$(QEMU) -machine $(QEMU_MACHINE) -nographic -bios none -kernel $<

# ==========
# Utilities
# ==========
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) kernel.elf kernel.bin

.PHONY: all clean run

Why this works for everyone

If someone’s toolchain prefix is different (e.g. riscv64-elf- instead of riscv64-unknown-elf-), they can just run:

make CROSS_COMPILE=riscv64-elf-
make CROSS_COMPILE=riscv64-elf- run


If QEMU isn’t named qemu-system-riscv64 on their system, they can do:

make QEMU=/path/to/qemu-system-riscv64 run


So the repo doesn’t care which OS; it only assumes that some RISC-V toolchain and QEMU exist and are reachable.

3. .gitignore for a clean repo

Create .gitignore at the root:

# Build artifacts
*.o
*.elf
*.bin

# Editor / OS junk
*.swp
*~
.DS_Store
.vscode/
.idea/


You can add more later (e.g. compile_commands.json, .clangd, etc.).

4. README.md – make it idiot-proof for teammates

Here’s a good starter README.md that makes onboarding easy:

# Simple RISC-V OS (CS Project)

This repo contains a tiny “OS” for the RISC-V `virt` board running in QEMU.
It boots, prints to the UART console, and provides a simple command-line shell.

Everyone should be able to build and run it with:

```bash
make
make run

Prerequisites

You need:

A RISC-V cross compiler (bare-metal):

e.g. riscv64-unknown-elf-gcc or riscv64-elf-gcc

QEMU with RISC-V support:

e.g. qemu-system-riscv64

See docs/SETUP.md for OS-specific installation instructions.

Building
make


This produces:

kernel.elf – ELF image of the kernel

kernel.bin – raw binary

You can override the toolchain prefix if yours is different:

make CROSS_COMPILE=riscv64-elf-

Running in QEMU
make run


Equivalently:

qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf


You should see something like:

Simple RISC-V OS booted!
Type 'help' for commands.

>


Type help, echo hello, etc.

If your QEMU binary has a different name or path:

make QEMU=/path/to/qemu-system-riscv64 run

Repo Layout

src/ – OS source code (boot, kernel, drivers)

linker.ld – linker script for RISC-V virt at 0x80000000

Makefile – build + run rules

docs/SETUP.md – per-OS setup instructions

docs/DESIGN.md – design notes, features, and roadmap

Contributing

Create a feature branch:

git checkout -b feature/scheduler

Commit changes with meaningful messages.

Open a PR to main and request review from the team.

See docs/DESIGN.md for TODOs and assigned features.


You and teammates can then expand `DESIGN.md` with features like scheduling, “multiple programs,” synchronization, etc.

---

## 5. `docs/SETUP.md` – per-OS setup cheatsheet

Create `docs/SETUP.md` like this, and teammates can tweak with their exact commands:

```markdown
# Setup Instructions

## Ubuntu / Other Linux

Install dependencies:

```bash
sudo apt update
sudo apt install qemu-system-misc gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf


Check:

qemu-system-riscv64 --version
riscv64-unknown-elf-gcc --version


Build and run:

make
make run


If your toolchain prefix is different, e.g. riscv64-elf-:

make CROSS_COMPILE=riscv64-elf-
make CROSS_COMPILE=riscv64-elf- run

macOS (Homebrew)

Install Homebrew (if needed) and then:

brew install qemu
# For toolchain, depending on tap:
# Example:
# brew tap riscv-software-src/riscv
# brew install riscv-tools  # or similar, your exact package may differ


Then identify your toolchain prefix (e.g. riscv64-elf-gcc) and run:

make CROSS_COMPILE=riscv64-elf-
make CROSS_COMPILE=riscv64-elf- run

Windows
Option 1: WSL2 (recommended)

Install WSL2 + Ubuntu.

Inside WSL, follow the Ubuntu instructions above.

Option 2: Native toolchain (MSYS2/Cygwin)

If someone really wants to do this, they should:

Install a RISC-V cross compiler and QEMU for Windows.

Add them to PATH.

Run from MSYS2/Cygwin shell using the same make commands.

Troubleshooting

qemu-system-riscv64: command not found
QEMU not installed or not on PATH. Install it or pass QEMU=/path/to/qemu-system-riscv64.

riscv64-unknown-elf-gcc: command not found
Toolchain missing. Install it or set CROSS_COMPILE to the right prefix.


You can fill in the exact macOS toolchain package once you or a teammate actually installs it.

---

## 6. Collaboration patterns (Git/GitHub)

Since it’s a group project, a sane workflow:

- **Main branch** = working, demoable code.
- Each feature = separate branch:
  - `feature/shell`
  - `feature/scheduler`
  - `feature/filesystem`
- PRs must:
  - Build (`make` succeeds)
  - Run at least the basic OS shell in QEMU
  - Update docs if behavior changes

You can even add a **GitHub Actions workflow** later to automatically run:

```bash
make
# Optional: run QEMU in headless mode for a few seconds


on every PR, but that’s frosting on the cake.

7. Next step for you right now

On your Ubuntu box:

Create the repo and folders:

mkdir riscv-os
cd riscv-os
mkdir -p src docs


Add:

Makefile (from above)

.gitignore

README.md

docs/SETUP.md

Move the OS source files we already sketched (start.S, kernel.c, uart.c, uart.h, linker.ld) into the right places.

Initialize Git + push to GitHub:

git init
git add .
git commit -m "Initial RISC-V OS skeleton"
git branch -M main
git remote add origin <your-repo-url>
git push -u origin main


After that, your teammates on macOS/Windows/Ubuntu should all be able to:

git clone <your-repo-url>
cd riscv-os
make
make run


If you want, next we can drop in the exact linker.ld, start.S, uart.*, and kernel.c files you’ll use so that the repo is “runnable” from the first commit.


chat 3(this is where I started to work)

input: 
can we setup this again but now I have the repo on my comptuer and the instr. to make the OS runnable ?

Output: 
Yep, let’s wire this up from your existing repo so you and your teammates can just git clone, make, make run.

I’ll assume:

You’re already inside the repo folder on your Ubuntu machine.

You’re okay creating the minimal OS we sketched (boot → UART → tiny shell).

0. Install the tools (once per machine)

On Ubuntu:

sudo apt update
sudo apt install qemu-system-misc gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf


Quick sanity check:

qemu-system-riscv64 --version
riscv64-unknown-elf-gcc --version


If both print versions, you’re set.

1. Create basic folders

From the root of your repo:

mkdir -p src docs

2. Add the core build files
2.1. Makefile (top level)

Create Makefile in the repo root:

cat > Makefile << 'EOF'
# ==========
# Toolchain
# ==========
CROSS_COMPILE ?= riscv64-unknown-elf-

CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS  = -march=rv64imac -mabi=lp64 -ffreestanding -nostdlib -nostartfiles -O2
LDFLAGS = -T linker.ld -nostdlib

# ==========
# QEMU
# ==========
QEMU         ?= qemu-system-riscv64
QEMU_MACHINE ?= virt

# ==========
# Files
# ==========
SRCS = src/kernel.c src/uart.c src/start.S
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

all: kernel.elf kernel.bin

kernel.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

run: kernel.elf
	$(QEMU) -machine $(QEMU_MACHINE) -nographic -bios none -kernel $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) kernel.elf kernel.bin

.PHONY: all clean run
EOF

2.2. .gitignore
cat > .gitignore << 'EOF'
# Build artifacts
*.o
*.elf
*.bin

# Editor / OS junk
*.swp
*~
.DS_Store
.vscode/
.idea/
EOF

2.3. linker.ld (top level)
cat > linker.ld << 'EOF'
ENTRY(_start)

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text.boot)
    *(.text*)
  }

  .rodata : {
    *(.rodata*)
  }

  .data : {
    *(.data*)
  }

  .bss : {
    *(.bss*)
    *(COMMON)
  }
}
EOF

3. Add the OS source files
3.1. src/uart.h
cat > src/uart.h << 'EOF'
#ifndef UART_H
#define UART_H

void uart_putc(char c);
void uart_puts(const char *s);
int  uart_getc_blocking(void);

#endif
EOF

3.2. src/uart.c
cat > src/uart.c << 'EOF'
#include <stdint.h>
#include "uart.h"

// UART0 base for QEMU virt machine
#define UART0_BASE 0x10000000UL

#define UART_THR       0x00  // Transmit Holding Register
#define UART_RBR       0x00  // Receiver Buffer Register
#define UART_LSR       0x05  // Line Status Register
#define UART_LSR_THRE  0x20  // Transmitter Holding Register Empty
#define UART_LSR_DR    0x01  // Data Ready

static inline uint8_t mmio_read8(uintptr_t addr) {
    return *(volatile uint8_t *)addr;
}

static inline void mmio_write8(uintptr_t addr, uint8_t val) {
    *(volatile uint8_t *)addr = val;
}

void uart_putc(char c) {
    // Wait until transmitter is ready
    while ((mmio_read8(UART0_BASE + UART_LSR) & UART_LSR_THRE) == 0)
        ;
    mmio_write8(UART0_BASE + UART_THR, (uint8_t)c);
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

int uart_getc_blocking(void) {
    // Wait until data is ready
    while ((mmio_read8(UART0_BASE + UART_LSR) & UART_LSR_DR) == 0)
        ;
    return mmio_read8(UART0_BASE + UART_RBR);
}
EOF

3.3. src/start.S
cat > src/start.S << 'EOF'
    .section .text.boot
    .globl _start
_start:
    # Set up a simple stack in .bss
    la sp, stack_top

    # Call C kernel main
    call kmain

1:  j 1b   # If kmain returns, loop forever

    .section .bss
    .align 16
stack:
    .space 4096 * 4     # 16KB stack
stack_top:
EOF

3.4. src/kernel.c
cat > src/kernel.c << 'EOF'
#include <stdint.h>
#include "uart.h"

static void read_line(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        int c = uart_getc_blocking();
        if (c == '\r' || c == '\n') {
            uart_putc('\r');
            uart_putc('\n');
            break;
        } else if (c == 0x7f || c == '\b') { // backspace
            if (i > 0) {
                i--;
                uart_puts("\b \b");
            }
        } else {
            buf[i++] = (char)c;
            uart_putc((char)c);
        }
    }
    buf[i] = '\0';
}

static int streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++; b++;
    }
    return *a == *b;
}

void kmain(void) {
    uart_puts("Simple RISC-V OS booted!\n");
    uart_puts("Type 'help' for commands.\n\n");

    char line[128];

    while (1) {
        uart_puts("> ");
        read_line(line, sizeof(line));

        if (streq(line, "help")) {
            uart_puts("Commands:\n");
            uart_puts("  help     - show this help\n");
            uart_puts("  echo ... - echo text\n");
            uart_puts("  reboot   - (not implemented, just message)\n");
        } else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' &&
                   line[3] == 'o' && (line[4] == ' ' || line[4] == '\0')) {
            // Skip "echo " (5 chars) if present
            const char *msg = line + 4;
            if (*msg == ' ')
                msg++;
            uart_puts(msg);
            uart_puts("\n");
        } else if (streq(line, "reboot")) {
            uart_puts("Pretending to reboot... (not implemented)\n");
        } else if (line[0] == '\0') {
            // empty line: ignore
        } else {
            uart_puts("Unknown command: ");
            uart_puts(line);
            uart_puts("\n");
        }
    }
}
EOF

4. Add basic docs
4.1. README.md
cat > README.md << 'EOF'
# Simple RISC-V OS (Course Project)

Tiny “OS” for the RISC-V `virt` board running in QEMU.

- Boots at 0x80000000
- Uses UART for console I/O
- Provides a simple command-line shell with `help`, `echo`, etc.

## Prerequisites (Ubuntu)

```bash
sudo apt update
sudo apt install qemu-system-misc gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf

Build
make


If your toolchain prefix differs (e.g. riscv64-elf-):

make CROSS_COMPILE=riscv64-elf-

Run in QEMU
make run


You should see:

Simple RISC-V OS booted!
Type 'help' for commands.

>


Type help, echo hello, etc.

If your QEMU binary has a different name or path:

make QEMU=/path/to/qemu-system-riscv64 run

Repo Layout

src/ — OS source code (boot, kernel, UART)

linker.ld — linker script for RISC-V virt at 0x80000000

Makefile — build + run rules

docs/ — setup / design notes (for the team)
EOF


---

### 4.2. `docs/SETUP.md` (optional but nice)

```bash
cat > docs/SETUP.md << 'EOF'
# Setup Instructions

## Ubuntu / Linux

```bash
sudo apt update
sudo apt install qemu-system-misc gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf


Then:

make
make run


If your toolchain prefix is different:

make CROSS_COMPILE=riscv64-elf-
make CROSS_COMPILE=riscv64-elf- run

macOS (Homebrew)

Install QEMU + a RISC-V toolchain (exact package may vary):

brew install qemu
# Then install a RISC-V cross compiler
# e.g., via a tap providing riscv64-elf-gcc


Then:

make CROSS_COMPILE=riscv64-elf-
make CROSS_COMPILE=riscv64-elf- run

Windows
Recommended: WSL2 + Ubuntu

Install WSL2 and Ubuntu.

Inside Ubuntu, follow the Linux steps.

EOF


---

## 5. Build and run locally

From the repo root:

```bash
make


You should see it compile and produce kernel.elf and kernel.bin.

Then:

make run


If everything’s wired up, your terminal should switch to QEMU serial and display:

Simple RISC-V OS booted!
Type 'help' for commands.

>


Try:

help

echo hello world

reboot (it just prints a message for now)

To quit QEMU: press Ctrl + A, then X.

6. Commit and push so the team can use it

Once it’s working:

git status
git add .
git commit -m "Initial RISC-V OS skeleton: boot + UART + shell"
git push origin main   # or whatever your main branch is called


Now your teammates can:

git clone <repo-url>
cd <repo>
make
make run


If you hit any build or QEMU errors after doing these exact steps, paste the error message and we’ll debug from there.

Chat 3: 
I am getting this error: "riscv64-unknown-elf-gcc -march=rv64imac -mabi=lp64 -ffreestanding -nostdlib -nostartfiles -O2 -T linker.ld -nostdlib -o kernel.elf src/kernel.o src/uart.o src/start.o src/kernel.o: in function kmain': kernel.c:(.text+0x0): relocation truncated to fit: R_RISCV_HI20 against .LC2' collect2: error: ld returned 1 exit status make: *** [Makefile:30: kernel.elf] Error 1" ?

Output: 

Nice, you just hit your first real RISC-V weirdo 🎉

That error:

relocation truncated to fit: R_RISCV_HI20 against `.LC2'


basically means:

“The compiler assumed all addresses would be below 2 GB, but you told the linker to put the code at 0x80000000, which is just above that, so I can’t encode the address.”

What’s going on?

Our linker.ld places the kernel at:

. = 0x80000000;


The default code model for RISC-V is medlow, which assumes all code/data are in the range 0x00000000–0x7fffffff (less than 2 GB).

But 0x80000000 is just outside that range (2³¹), so some instructions that use the HI20 relocation can’t encode it → relocation truncated error.

We need to tell GCC:

“Hey, my code/data might live anywhere, not just below 2 GB.”

That’s what -mcmodel=medany does.

✅ Fix: update your Makefile CFLAGS

Open your Makefile and change this line:

CFLAGS  = -march=rv64imac -mabi=lp64 -ffreestanding -nostdlib -nostartfiles -O2


to this:

CFLAGS  = -march=rv64imac -mabi=lp64 -mcmodel=medany -ffreestanding -nostdlib -nostartfiles -O2


So the top of your Makefile should look like:

CROSS_COMPILE ?= riscv64-unknown-elf-

CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS  = -march=rv64imac -mabi=lp64 -mcmodel=medany -ffreestanding -nostdlib -nostartfiles -O2
LDFLAGS = -T linker.ld -nostdlib

Rebuild

From the repo root:

make clean
make
make run


If everything else is wired correctly, you should now see:

Simple RISC-V OS booted!
Type 'help' for commands.

>


Try help and echo hi.

If you get any new error messages after this change, paste them and we’ll keep debugging.