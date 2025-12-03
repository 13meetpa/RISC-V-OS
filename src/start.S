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

