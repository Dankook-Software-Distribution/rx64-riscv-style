// Physical memory layout

// qemu -machine q35 is set up like this,
// based on qemu's hw/i386/pc_q35.c:
//
// 00000000 -- RAM, but the first 1MB holds the BIOS
//             and other legacy areas
// 00100000 -- qemu's -kernel loads the kernel here,
//             then jumps to _entry (PVH).
// 08000000 -- end of RAM (-m 128M)
// ...      -- PCI devices' registers, at addresses
//             chosen by the firmware (FE000000 for
//             the virtio disk)
// FEC00000 -- IOAPIC
// FEE00000 -- local APIC (LAPIC), one per CPU,
//             each CPU sees its own at this address
// the UART is not in this list: it is reached
// through I/O ports, not memory addresses.

// the kernel uses physical memory thus:
// 00100000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// the PC's first serial port (COM1): its registers
// are at these I/O ports (not memory), and it raises
// interrupts on IOAPIC input (GSI) 4.
#define UART0     0x3f8
#define UART0_IRQ 4

// virtio disk: a virtio-blk-pci device (modern only),
// found by its PCI vendor and device IDs. the firmware
// chooses the addresses of its registers (see pci.c).
// qemu puts it in PCI slot 3 (-device ...,addr=0x3), and
// the ACPI tables (DSDT _PRT) connect slot 3's INTA pin
// to IOAPIC input (GSI) 20 + (3 + 0) % 4 = 23.
#define VIRTIO0_VENDOR 0x1af4
#define VIRTIO0_DEVICE 0x1042
#define VIRTIO0_IRQ    23

// local interrupt controller (LAPIC), riscv's CLINT.
#define LAPIC 0xfee00000L

// I/O interrupt controller (IOAPIC), riscv's PLIC.
#define IOAPIC 0xfec00000L

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address KERNBASE to PHYSTOP.
// a PC's RAM starts at 0, but the first 1MB holds
// the BIOS and other legacy areas, so the kernel
// starts at 1MB; RAM ends at 128MB (qemu -m 128M).
#define KERNBASE 0x100000L
#define PHYSTOP  (128 * 1024 * 1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline and the
// cpu entry pages (CPUENTRY, below),
// each surrounded by invalid guard pages.
#define KSTACK(p) (CPUENTRY - ((p) + 1) * 2 * PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   CPUENTRY (two pages, the same as in the kernel)
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

// riscv finds the trap handler through stvec, a register.
// x86 finds it through tables in memory: the GDT, the TSS
// (for the kernel stack) and the IDT, at virtual addresses
// that the CPU looks up in whatever page table is in use
// when a trap arrives. so, like the trampoline, these
// tables are mapped at the same address in user and
// kernel space, in two pages beneath TRAPFRAME: one for
// every CPU's GDT and TSS, one for the user IDT.
// user memory must end below CPUENTRY.
#define CPUENTRY (TRAPFRAME - 2 * PGSIZE)
