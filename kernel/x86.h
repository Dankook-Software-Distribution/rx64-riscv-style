// x86-64 control registers and model-specific registers.
// these are plain numbers (no C integer suffixes) so that
// assembly files such as entry.S can use them too.

#define CR0_PE 0x00000001 // protected mode enable
#define CR0_WP 0x00010000 // kernel obeys read-only PTEs too
#define CR0_PG 0x80000000 // paging enable

#define CR4_PAE 0x00000020 // physical address extension (64-bit PTEs)

#define FL_IF 0x00000200 // rflags: interrupt enable

#define MSR_EFER 0xc0000080 // extended feature enable register
#define EFER_LME 0x00000100 // long mode enable
#define EFER_NXE 0x00000800 // no-execute PTE bit enable

#define MSR_GS_BASE        0xc0000101 // base address of the gs segment
#define MSR_KERNEL_GS_BASE 0xc0000102 // swapgs exchanges it with MSR_GS_BASE

// global descriptor table (GDT) entries.
// a segment selector is a GDT index times 8, plus the
// privilege level it requests (0 kernel, 3 user).
#define GDT_KCODE 1 // kernel code
#define GDT_KDATA 2 // kernel data and stack
#define GDT_UDATA 3 // user data and stack
#define GDT_UCODE 4 // user code
#define GDT_TSS   5 // task state segment, takes two entries
#define NGDT      7

#define KCODE_SEL (GDT_KCODE << 3)
#define KDATA_SEL (GDT_KDATA << 3)
#define UDATA_SEL ((GDT_UDATA << 3) | 3)
#define UCODE_SEL ((GDT_UCODE << 3) | 3)
#define TSS_SEL   (GDT_TSS << 3)

// x86 exception vectors (riscv's scause exception codes).
// vectors 32..255 are free for device interrupts and
// system calls.
#define T_DIVIDE  0  // divide error
#define T_DEBUG   1  // debug
#define T_NMI     2  // non-maskable interrupt
#define T_BRKPT   3  // breakpoint
#define T_OFLOW   4  // overflow
#define T_BOUND   5  // bounds check
#define T_ILLOP   6  // illegal opcode
#define T_NOFPU   7  // device (FPU) not available
#define T_DBLFLT  8  // double fault
#define T_TSS     10 // invalid task switch segment
#define T_SEGNP   11 // segment not present
#define T_STACK   12 // stack exception
#define T_GPFLT   13 // general protection fault
#define T_PGFLT   14 // page fault
#define T_FPERR   16 // floating point error
#define T_ALIGN   17 // alignment check
#define T_MCHK    18 // machine check
#define T_SIMDERR 19 // SIMD floating point error

// device interrupt vectors (riscv's scause interrupt codes).
// the IOAPIC raises vector T_IRQ0 + n for its input (GSI) n,
// so devintr() can find the IRQ number without asking the
// interrupt controller, as riscv's plic_claim() does.
#define T_IRQ0     32
#define T_TIMER    (T_IRQ0 + 0) // LAPIC timer. GSI 0 is the
                                // 8259's, which is masked.
#define T_SPURIOUS 0xff         // LAPIC spurious interrupt

// page fault error code bits.
#define PFERR_P 0x1  // 1: protection violation, 0: page not present
#define PFERR_W 0x2  // the access was a write
#define PFERR_U 0x4  // the access came from user mode
#define PFERR_I 0x10 // the access was an instruction fetch

#ifndef __ASSEMBLER__

// x86 devices such as the UART are reached through
// a separate I/O port address space, with in/out
// instructions instead of loads and stores.
static inline uchar
inb(ushort port)
{
  uchar x;
  asm volatile("inb %1, %0" : "=a"(x) : "d"(port));
  return x;
}

static inline void
outb(ushort port, uchar x)
{
  asm volatile("outb %0, %1" : : "a"(x), "d"(port));
}

static inline ushort
inw(ushort port)
{
  ushort x;
  asm volatile("inw %1, %0" : "=a"(x) : "d"(port));
  return x;
}

static inline void
outw(ushort port, ushort x)
{
  asm volatile("outw %0, %1" : : "a"(x), "d"(port));
}

static inline uint32
inl(ushort port)
{
  uint32 x;
  asm volatile("inl %1, %0" : "=a"(x) : "d"(port));
  return x;
}

static inline void
outl(ushort port, uint32 x)
{
  asm volatile("outl %0, %1" : : "a"(x), "d"(port));
}

// which CPU is this? (riscv's mhartid)
// cpuid leaf 1 reports this CPU's initial local APIC ID in
// ebx bits 31..24. qemu numbers them 0, 1, 2, ..., so the
// APIC ID can index cpus[].
static inline int
r_apicid()
{
  uint32 a = 1, b, c = 0, d;
  asm volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
  return b >> 24;
}

// model-specific registers.
static inline uint64
rdmsr(uint32 msr)
{
  uint32 lo, hi;
  asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
  return ((uint64)hi << 32) | lo;
}

static inline void
wrmsr(uint32 msr, uint64 x)
{
  asm volatile("wrmsr" : : "c"(msr), "a"((uint32)x), "d"((uint32)(x >> 32)));
}

// read the 8 bytes at the gs segment's base address.
// the kernel keeps a pointer to this CPU's struct cpu there
// (see start.c), for mycpu().
static inline uint64
r_gsptr()
{
  uint64 x;
  asm volatile("movq %%gs:0, %0" : "=r"(x));
  return x;
}

// which hart (core) is this?
static inline uint64
r_mhartid()
{
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r"(x));
  return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M    (3L << 11)
#define MSTATUS_MPP_S    (1L << 11)
#define MSTATUS_MPP_U    (0L << 11)

static inline uint64
r_mstatus()
{
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r"(x));
  return x;
}

static inline void
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r"(x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r"(x));
}

// Supervisor Status Register, sstatus

#define SSTATUS_SPP  (1L << 8) // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE  (1L << 1) // Supervisor Interrupt Enable
#define SSTATUS_UIE  (1L << 0) // User Interrupt Enable

static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r"(x));
  return x;
}

static inline void
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r"(x));
}

static inline void
s_sstatus(uint64 x)
{
  __asm__ __volatile__("csrs sstatus, %0" ::"rK"(x) : "memory");
}

static inline void
c_sstatus(uint64 x)
{
  __asm__ __volatile__("csrc sstatus, %0" ::"rK"(x) : "memory");
}

// Supervisor Interrupt Pending
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r"(x));
  return x;
}

static inline void
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r"(x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r"(x));
  return x;
}

static inline void
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r"(x));
}

// Machine-mode Interrupt Enable
#define MIE_STIE (1L << 5) // supervisor timer
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r"(x));
  return x;
}

static inline void
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r"(x));
}

// supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r"(x));
}

static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r"(x));
  return x;
}

// Machine Exception Delegation
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r"(x));
  return x;
}

static inline void
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r"(x));
}

// Machine Interrupt Delegation
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r"(x));
  return x;
}

static inline void
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r"(x));
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r"(x));
}

static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r"(x));
  return x;
}

// Supervisor Timer Comparison Register
static inline uint64
r_stimecmp()
{
  uint64 x;
  // asm volatile("csrr %0, stimecmp" : "=r" (x) );
  asm volatile("csrr %0, 0x14d" : "=r"(x));
  return x;
}

static inline void
w_stimecmp(uint64 x)
{
  // asm volatile("csrw stimecmp, %0" : : "r" (x));
  asm volatile("csrw 0x14d, %0" : : "r"(x));
}

// Machine Environment Configuration Register

#define MENVCFG_STCE (1L << 63)
#define MENVCFG_ADUE (1L << 61)

static inline uint64
r_menvcfg()
{
  uint64 x;
  // asm volatile("csrr %0, menvcfg" : "=r" (x) );
  asm volatile("csrr %0, 0x30a" : "=r"(x));
  return x;
}

static inline void
w_menvcfg(uint64 x)
{
  // asm volatile("csrw menvcfg, %0" : : "r" (x));
  asm volatile("csrw 0x30a, %0" : : "r"(x));
}

// Physical Memory Protection
static inline void
w_pmpcfg0(uint64 x)
{
  asm volatile("csrw pmpcfg0, %0" : : "r"(x));
}

static inline void
w_pmpaddr0(uint64 x)
{
  asm volatile("csrw pmpaddr0, %0" : : "r"(x));
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
static inline void
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r"(x));
}

static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r"(x));
  return x;
}

// Supervisor Trap Cause
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r"(x));
  return x;
}

// Supervisor Trap Value
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r"(x));
  return x;
}

// Machine-mode Counter-Enable
static inline void
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r"(x));
}

static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r"(x));
  return x;
}

// machine-mode cycle counter
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r"(x));
  return x;
}

static inline uint64
r_rflags()
{
  uint64 x;
  asm volatile("pushfq; popq %0" : "=r"(x));
  return x;
}

// enable device interrupts
static inline void
intr_on()
{
  asm volatile("sti" ::: "memory");
}

// disable device interrupts
static inline void
intr_off()
{
  asm volatile("cli" ::: "memory");
}

// are device interrupts enabled?
static inline int
intr_get()
{
  uint64 x = r_rflags();
  return (x & FL_IF) != 0;
}

static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r"(x));
  return x;
}

// read and write tp, the thread pointer, which xv6 uses to hold
// this core's hartid (core number), the index into cpus[].
static inline uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r"(x));
  return x;
}

static inline void
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r"(x));
}

static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r"(x));
  return x;
}

// control register 3 holds the physical address
// of the page table (riscv's satp).
// writing it also flushes the TLB.
static inline void
w_cr3(uint64 x)
{
  asm volatile("movq %0, %%cr3" : : "r"(x) : "memory");
}

static inline uint64
r_cr3()
{
  uint64 x;
  asm volatile("movq %%cr3, %0" : "=r"(x));
  return x;
}

// control register 2 holds the virtual address
// that caused the last page fault (riscv's stval).
static inline uint64
r_cr2()
{
  uint64 x;
  asm volatile("movq %%cr2, %0" : "=r"(x));
  return x;
}

// GDT segment descriptor bits.
// long mode ignores the base and limit of code and data
// segments; only these bits matter.
#define SEG_A        (1UL << 40) // accessed; set so the CPU never writes the GDT
#define SEG_W        (1UL << 41) // data: writable (code: readable)
#define SEG_X        (1UL << 43) // code segment
#define SEG_S        (1UL << 44) // code or data, not a system segment
#define SEG_DPL(dpl) ((uint64)(dpl) << 45)
#define SEG_P        (1UL << 47) // present
#define SEG_L        (1UL << 53) // 64-bit code

#define SEG_TSS64 (9UL << 40) // system segment type: available 64-bit TSS

// task state segment. x86 has no use for its task switching,
// but the CPU reads trap stack pointers from it: rsp[0] when a
// trap comes from user mode, ist[n] for IDT gates that name an
// interrupt stack table entry n (1..7).
// ist[0] does not exist; that slot is reserved.
struct tss {
  uint32 reserved0;
  uint64 rsp[3];
  uint64 ist[8];
  uint64 reserved1;
  uint16 reserved2;
  uint16 iomb; // I/O permission bitmap offset
} __attribute__((packed));

// interrupt descriptor table (IDT) gate: the address of the
// code for one vector, like riscv's stvec but one per vector.
struct gatedesc {
  uint16 off_15_0;
  uint16 cs;       // code segment selector
  uint8 ist;       // interrupt stack table entry, 0 for none
  uint8 type;      // GATE_ bits
  uint16 off_31_16;
  uint32 off_63_32;
  uint32 reserved;
};

// an interrupt gate turns interrupts off on entry, as a riscv
// trap does. xv6 uses only interrupt gates.
#define GATE_INTR       0x0e
#define GATE_DPL(dpl)   ((dpl) << 5) // lowest privilege allowed to "int" to it
#define GATE_P          0x80         // present

// load the GDT, IDT or task register.
// lgdt and lidt take a 10-byte operand: limit, then base.
struct dtr {
  uint16 limit;
  uint64 base;
} __attribute__((packed));

static inline void
lgdt(void *p, int size)
{
  volatile struct dtr d = {size - 1, (uint64)p};
  asm volatile("lgdt %0" : : "m"(d));
}

static inline void
lidt(void *p, int size)
{
  volatile struct dtr d = {size - 1, (uint64)p};
  asm volatile("lidt %0" : : "m"(d));
}

static inline void
ltr(uint16 sel)
{
  asm volatile("ltr %0" : : "r"(sel));
}

// lgdt does not reload the segment registers, which still
// hold descriptors from the old GDT. load cs with a far
// return, and ds, es and ss directly. fs and gs are left
// alone, so that their base addresses are kept.
static inline void
loadsegs(uint16 cs, uint16 ds)
{
  asm volatile("pushq %q0\n\t"
               "leaq 1f(%%rip), %%rax\n\t"
               "pushq %%rax\n\t"
               "lretq\n"
               "1:\n\t"
               "movw %w1, %%ds\n\t"
               "movw %w1, %%es\n\t"
               "movw %w1, %%ss"
               :
               : "r"((uint64)cs), "r"((uint64)ds)
               : "rax", "memory");
}

// flush the TLB.
static inline void
sfence_vma()
{
  // the zero, zero means flush all TLB entries.
  asm volatile("sfence.vma zero, zero" ::: "memory");
}

// fence for memory-mapped IO.
// riscv needs a fence instruction here, but x86 keeps the
// order that matters to a device: stores to RAM become
// visible (to the CPUs and to a device reading RAM) in
// program order, a store to a device register is not done
// before earlier stores, and loads are not reordered with
// other loads. so only the compiler must be kept from
// reordering memory accesses across this point.
// (device registers must be uncached for this; the
// firmware's MTRRs make the PCI and APIC addresses so.)
static inline void
io_fence()
{
  asm volatile("" ::: "memory");
}

// fence for icache
static inline void
icache_fence()
{
  asm volatile("fence.i" ::: "memory");
}

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

#endif // __ASSEMBLER__

#define PGSIZE  4096 // bytes per page
#define PGSHIFT 12   // bits of offset within a page

#define PGROUNDUP(sz)  (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))

// x86-64 PTEs have no read or execute permission bits:
// every present page is readable, and a page is executable
// unless its no-execute bit (PTE_NX) is set. the kernel
// still uses riscv's permissions, keeping PTE_R and PTE_X
// in bits that the MMU ignores. mappages() sets PTE_NX
// for mappings without PTE_X.
#define PTE_V  (1L << 0)   // valid (x86 "present")
#define PTE_W  (1L << 1)
#define PTE_U  (1L << 2)   // user can access
#define PTE_R  (1L << 9)   // ignored by the MMU
#define PTE_X  (1L << 10)  // ignored by the MMU
#define PTE_NX (1UL << 63) // no-execute; needs EFER.NXE (entry.S)

// an x86-64 PTE holds a physical address in bits 12..51,
// at the same position as in the address itself.
#define PTE_ADDR 0x000FFFFFFFFFF000L

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) (((uint64)pa) & PTE_ADDR)

#define PTE2PA(pte) ((pte) & PTE_ADDR)

#define PTE_FLAGS(pte) ((pte) & ~PTE_ADDR)

// extract the four 9-bit page table indices from a virtual address.
#define PXMASK         0x1FF // 9 bits
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va)  ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// four-level paging, to avoid having to sign-extend virtual
// addresses that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 9 + 12 - 1))
