        # qemu -kernel loads the kernel at 0x100000
        # and, because of the PVH ELF note below,
        # makes the boot CPU jump to _entry in 32-bit
        # protected mode with paging off and interrupts
        # disabled. the other CPUs stay halted until
        # the kernel starts them.
        # kernel.ld causes the following code to
        # be placed at 0x100000.

#include "x86.h"

// selectors in bootgdt.
#define BOOT_KCODE 0x08
#define BOOT_KDATA 0x10

// PTE bits for the temporary page table.
#define BOOT_PTE_P  0x001 // present
#define BOOT_PTE_W  0x002 // writable
#define BOOT_PTE_PS 0x080 // 2MB page

        # PVH ELF note: XEN_ELFNOTE_PHYS32_ENTRY (18)
        # holds the 32-bit physical address of _entry.
.section .note.Xen, "a", @note
        .p2align 2
        .long 4                 # name size
        .long 4                 # descriptor size
        .long 18                # type
        .asciz "Xen"            # name
        .long _entry            # descriptor

.section .text
.code32
.global _entry
_entry:
        # riscv starts main() with paging off (satp = 0),
        # but x86-64 long mode cannot run without paging.
        # so turn on paging with bootpml4, a temporary page
        # table that maps the low 1GB of physical memory at
        # the same virtual addresses (identity mapping).
        # main() switches to the real kernel page table
        # in kvminithart().

        # long mode uses 64-bit (PAE format) PTEs.
        movl %cr4, %eax
        orl $CR4_PAE, %eax
        movl %eax, %cr4

        movl $bootpml4, %eax
        movl %eax, %cr3

        # enable long mode, and the PTE no-execute bit
        # that the kernel page table will use.
        movl $MSR_EFER, %ecx
        rdmsr
        orl $(EFER_LME | EFER_NXE), %eax
        wrmsr

        # turning on paging activates long mode.
        # WP makes the kernel obey read-only PTEs, as
        # riscv's supervisor mode does.
        movl %cr0, %eax
        orl $(CR0_PG | CR0_WP), %eax
        movl %eax, %cr0

        # the CPU still runs 32-bit code until it loads
        # a 64-bit code segment. the PVH loader leaves
        # no usable GDT, so load bootgdt and far jump
        # into its 64-bit code segment.
        lgdt bootgdtdesc
        ljmp $BOOT_KCODE, $entry64

.code64
entry64:
        # long mode ignores the data segment bases and
        # limits, but the registers must still hold
        # valid selectors.
        movw $BOOT_KDATA, %ax
        movw %ax, %ds
        movw %ax, %es
        movw %ax, %ss
        xorw %ax, %ax
        movw %ax, %fs
        movw %ax, %gs

        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # rsp = stack0 + ((apicid + 1) * 4096)
        # riscv reads the hartid from mhartid; x86
        # has the LAPIC ID in cpuid leaf 1, EBX[31:24].
        movl $1, %eax
        cpuid
        shrl $24, %ebx
        addl $1, %ebx
        shll $12, %ebx
        movq $stack0, %rsp
        addq %rbx, %rsp
        # jump to start() in start.c
        call start
spin:
        jmp spin

        # temporary GDT for the jump to 64-bit mode.
        # the kernel installs its own GDT later.
.section .rodata
.p2align 3
bootgdt:
        .quad 0                         # null
        .quad 0x00af9a000000ffff        # 0x08: kernel code, 64-bit
        .quad 0x00cf92000000ffff        # 0x10: kernel data
bootgdtdesc:
        .word bootgdtdesc - bootgdt - 1 # limit
        .long bootgdt                   # base

        # temporary page table for entry.S:
        # one PML4 entry -> one PDPT entry -> one
        # page directory of 512 2MB pages = 1GB.
.section .data
.p2align 12
bootpml4:
        .quad bootpdpt + (BOOT_PTE_P | BOOT_PTE_W)
        .fill 511, 8, 0
bootpdpt:
        .quad bootpd + (BOOT_PTE_P | BOOT_PTE_W)
        .fill 511, 8, 0
bootpd:
        .set i, 0
        .rept 512
        .quad (i << 21) | (BOOT_PTE_P | BOOT_PTE_W | BOOT_PTE_PS)
        .set i, i + 1
        .endr
