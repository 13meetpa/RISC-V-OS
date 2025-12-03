/* src/user_calc.S - tiny user-space calculator program (fixed)
   Saves ra and t0 in non-leaf functions so nested calls don't clobber them.
   Linked/loaded at 0x80200000 (user_linker.ld should place .text there)
*/

    .section .text
    .globl _start
    .align  4

#define UART_BASE 0x10000000
#define UART_RHR  0
#define UART_THR  0
#define UART_LSR  5
#define LSR_THRE  0x20
#define LSR_DR    0x01

_start:
    /* Print prompt by calling print_string (print_string preserves ra and t0) */
    la a0, msg_prompt
    jal ra, print_string

    /* read first digit */
    jal ra, uart_getc
    mv t0, a0        /* t0 = first char */
    jal ra, uart_putc

    /* read operator */
    jal ra, uart_getc
    mv t1, a0        /* t1 = operator */
    jal ra, uart_putc

    /* read second digit */
    jal ra, uart_getc
    mv t2, a0        /* t2 = second char */
    jal ra, uart_putc

    /* newline */
    li a0, '\n'
    jal ra, uart_putc

    /* convert ASCII digits to integers */
    li t3, '0'
    sub t0, t0, t3
    sub t2, t2, t3

    /* compute result */
    li t3, '+'
    beq t1, t3, do_add
    li t3, '-'
    beq t1, t3, do_sub
    li t3, '*'
    beq t1, t3, do_mul
    li t3, '/'
    beq t1, t3, do_div

    /* unknown operator -> result = 0 */
    li t4, 0
    j calc_done

do_add:
    add t4, t0, t2
    j calc_done

do_sub:
    sub t4, t0, t2
    j calc_done

do_mul:
    mul t4, t0, t2
    j calc_done

do_div:
    beqz t2, div_zero
    div t4, t0, t2
    j calc_done

div_zero:
    li t4, 0

calc_done:
    la a0, msg_result
    jal ra, print_string

    /* print number (support 0..99) */
    mv t0, t4
    li t1, 10
    div t2, t0, t1    /* tens */
    rem t3, t0, t1    /* ones */

    beqz t2, print_ones
    addi a0, t2, '0'
    jal ra, uart_putc

print_ones:
    addi a0, t3, '0'
    jal ra, uart_putc

    li a0, '\n'
    jal ra, uart_putc

    ret


/* --------------------------------------------------------- */
/* UART helpers                                              */
/* --------------------------------------------------------- */

uart_putc:
    li t0, UART_BASE
1:
    lb t1, UART_LSR(t0)
    li t2, LSR_THRE
    and t1, t1, t2
    beqz t1, 1b
    sb a0, UART_THR(t0)
    ret

uart_getc:
    li t0, UART_BASE
2:
    lb t1, UART_LSR(t0)
    li t2, LSR_DR
    and t1, t1, t2
    beqz t1, 2b
    lb a0, UART_RHR(t0)
    ret

/* print_string now preserves ra AND t0 on the stack (non-leaf) */
print_string:
    addi sp, sp, -16     /* make stack frame */
    sd t0, 0(sp)         /* save t0 */
    sd ra, 8(sp)         /* save ra */

    mv t0, a0            /* t0 := string pointer */
3:
    lbu t1, 0(t0)
    beqz t1, 4f
    mv a0, t1
    jal ra, uart_putc
    addi t0, t0, 1
    j 3b
4:
    ld t0, 0(sp)         /* restore t0 */
    ld ra, 8(sp)         /* restore ra */
    addi sp, sp, 16
    ret

/* Strings */
    .section .rodata
msg_prompt:
    .ascii "Calc: enter a+b\n\0"

msg_result:
    .ascii "Result = \0"
