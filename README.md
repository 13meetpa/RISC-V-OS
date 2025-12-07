# RISC-V-OS
This is RISC-V Command line OS. 



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

To run the OS use:

make

make run

To clean the files use:
make clean

Group memeber names
Meetkumar Patel 
Evan Ash
Zachary Sansone
Ian Attmore


