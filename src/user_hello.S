/* src/user_hello.S - tiny user program that prints via UART and returns */
    .section .text
    .global _start
    .align  4
_start:
    la a0, msg        /* a0 -> message pointer */

1:  lbu t0, 0(a0)
    beqz t0, 2f       /* done if zero */
    li t1, 0x10000000 /* UART base */

wait_thr:
    lb t2, 5(t1)      /* UART_LSR offset 5 */
    li t3, 0x20       /* THR empty bit */
    and t2, t2, t3
    beqz t2, wait_thr

    sb t0, 0(t1)      /* UART_THR offset 0 */
    addi a0, a0, 1
    j 1b

2:
    /* return control to kernel — loader sets up ra so 'ret' returns to kernel */
    ret

    .section .rodata
msg:
    .ascii "Hello from user program!\n\0"
