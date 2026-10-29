        #
        # interrupts and exceptions while in kernel
        # mode come here.
        #
        # riscv jumps to the one address in stvec for every
        # trap. x86 instead jumps through the IDT to separate
        # code for each of its 256 vectors, so trapinit()
        # points every IDT entry at one of the small stubs
        # below; each stub pushes its vector number and
        # jumps to kernelvec.
        #
        # the CPU has already pushed ss, rsp, rflags, cs and
        # rip on the current kernel stack, and, for some
        # vectors, an error code. it does not switch stacks
        # or change gs for traps from kernel mode; traps from
        # user mode go to uservec in trampoline.S instead.
        #

#include "x86.h"

        # the vectors for which the CPU pushes an error code
        # (21, 29 and 30 are the control protection, VMM
        # communication and security exceptions). the others
        # push a 0 in its place, so that every trap leaves
        # the same frame on the stack.
.macro vecstub n
vec\n:
        .if (\n == T_DBLFLT) || (\n >= T_TSS && \n <= T_PGFLT) || (\n == T_ALIGN) || (\n == 21) || (\n == 29) || (\n == 30)
        .else
        pushq $0
        .endif
        pushq $\n
        jmp kernelvec
.endm

.altmacro

.section .text
.set i, 0
.rept 256
        vecstub %i
.set i, i + 1
.endr

        # kernelvecs[i] is the address of vector i's stub,
        # for trapinit() to put in the IDT.
.macro vecaddr n
        .quad vec\n
.endm

.section .rodata
.p2align 3
.globl kernelvecs
kernelvecs:
.set i, 0
.rept 256
        vecaddr %i
.set i, i + 1
.endr

.noaltmacro

        #
        # the current stack is a kernel stack.
        # push registers, call kerneltrap().
        # when kerneltrap() returns, restore registers, return.
        #
.section .text
.globl kerneltrap
.globl kernelvec
.p2align 4
kernelvec:
        # make room to save registers.
        subq $72, %rsp

        # save caller-saved registers.
        # the stack now holds, from rsp upward:
        #     0 rax     8 rcx    16 rdx    24 rsi
        #    32 rdi    40 r8     48 r9     56 r10
        #    64 r11    72 vector 80 error code
        #    88 rip    96 cs    104 rflags 112 rsp  120 ss
        # (the CPU aligned rsp to 16 bytes before pushing
        # ss, so rsp is 16-byte aligned again here, as the
        # C calling convention needs.)
        movq %rax, 0(%rsp)
        movq %rcx, 8(%rsp)
        movq %rdx, 16(%rsp)
        movq %rsi, 24(%rsp)
        movq %rdi, 32(%rsp)
        movq %r8, 40(%rsp)
        movq %r9, 48(%rsp)
        movq %r10, 56(%rsp)
        movq %r11, 64(%rsp)

        # call the C trap handler in trap.c,
        # with a pointer to the saved frame.
        # riscv's kerneltrap() reads the vector, the
        # previous pc and the previous mode from CSRs;
        # x86 leaves them in this frame instead.
        movq %rsp, %rdi
        call kerneltrap

        # restore registers.
        movq 0(%rsp), %rax
        movq 8(%rsp), %rcx
        movq 16(%rsp), %rdx
        movq 24(%rsp), %rsi
        movq 32(%rsp), %rdi
        movq 40(%rsp), %r8
        movq 48(%rsp), %r9
        movq 56(%rsp), %r10
        movq 64(%rsp), %r11

        # pop the registers, the vector and the error code.
        addq $(72 + 16), %rsp

        # return to whatever we were doing in the kernel.
        iretq
