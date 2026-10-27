        #
        # low-level code to handle traps from user space into
        # the kernel, and returns from kernel to user.
        #
        # the kernel maps the page holding this code
        # at the same virtual address (TRAMPOLINE)
        # in user and kernel space so that it continues
        # to work when it switches page tables.
        # kernel.ld causes this code to start at
        # a page boundary.
        #

#include "x86.h"
#include "memlayout.h"

.section trampsec
.globl trampoline
.globl usertrap
trampoline:

        #
        # prepare_return() loads the user IDT, whose gates
        # point at these entry stubs (at their addresses in
        # the TRAMPOLINE page), so traps from user space
        # start here, in kernel mode, but with a user page
        # table.
        #
        # riscv jumps to the one address in stvec for every
        # trap; x86 needs code for each of its 256 vectors,
        # as in kernelvec.S. each stub pushes a 0 if the CPU
        # pushed no error code, then its vector number, and
        # jumps to uservec.
        #
.macro uvecstub n
uvec\n:
        .if (\n == T_DBLFLT) || (\n >= T_TSS && \n <= T_PGFLT) || (\n == T_ALIGN) || (\n == 21) || (\n == 29) || (\n == 30)
        .else
        pushq $0
        .endif
        pushq $\n
        jmp uservec
.endm

.altmacro

.set i, 0
.rept 256
        uvecstub %i
.set i, i + 1
.endr

.globl uservec
uservec:
        #
        # the CPU switched to the stack in the TSS (rsp[0]),
        # which ends at the end of this process's trapframe
        # (see trapinithart()), and pushed the user's ss,
        # rsp, rflags, cs and rip there. with the vector and
        # the error code from the stub, the trapframe now
        # holds everything from vector on.
        #

        # make gs point to this CPU's struct cpu, for mycpu();
        # swapgs swaps the user's gs base with the one that
        # userret saved in MSR_KERNEL_GS_BASE.
        # (riscv loads tp from p->trapframe->kernel_hartid.)
        swapgs

        # save the user registers in the trapframe,
        # below the vector.
        pushq %rax
        pushq %rbx
        pushq %rcx
        pushq %rdx
        pushq %rsi
        pushq %rdi
        pushq %rbp
        pushq %r8
        pushq %r9
        pushq %r10
        pushq %r11
        pushq %r12
        pushq %r13
        pushq %r14
        pushq %r15

        # each process has a separate p->trapframe memory area,
        # but it's mapped to the same virtual address
        # (TRAPFRAME) in every process's user page table.
        movabsq $TRAPFRAME, %rax

        # initialize kernel stack pointer, from p->trapframe->kernel_sp
        movq 8(%rax), %rsp

        # load the address of usertrap(), from p->trapframe->kernel_trap
        movq 16(%rax), %rbx

        # fetch the kernel page table address, from p->trapframe->kernel_cr3.
        movq 0(%rax), %rcx

        # install the kernel page table.
        # writing cr3 also flushes the user's TLB entries.
        movq %rcx, %cr3

        # the user may have set the direction flag, but C
        # code expects it clear. (the trap turned interrupts
        # off, as a riscv trap does, but left this flag.)
        cld

        # call usertrap(); it returns to userret, below.
        call *%rbx

        # usertrap() returns here, with the user page table
        # in rax. userret takes it in rdi, the first
        # argument, since forkret() calls userret as a
        # C function.
        movq %rax, %rdi

.globl userret
userret:
        # return from kernel to user.

        # switch to the user page table.
        movq %rdi, %cr3

        # restore the user registers from TRAPFRAME.
        movabsq $(TRAPFRAME + 32), %rsp
        popq %r15
        popq %r14
        popq %r13
        popq %r12
        popq %r11
        popq %r10
        popq %r9
        popq %r8
        popq %rbp
        popq %rdi
        popq %rsi
        popq %rdx
        popq %rcx
        popq %rbx
        popq %rax

        # skip the vector and error code.
        addq $16, %rsp

        # give the user its gs base back, and save this
        # CPU's for the next uservec.
        swapgs

        # return to user mode and user pc.
        # prepare_return() set up the trapframe's
        # rip, cs, rflags, rsp and ss for iretq.
        iretq

.noaltmacro

        # uservecs[i] is the kernel address of vector i's
        # stub, for trapinit() to turn into its address in
        # the TRAMPOLINE page and put in the user IDT.
.macro uvecaddr n
        .quad uvec\n
.endm

.altmacro

.section .rodata
.p2align 3
.globl uservecs
uservecs:
.set i, 0
.rept 256
        uvecaddr %i
.set i, i + 1
.endr

.noaltmacro
