#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "x86.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[];

// in kernelvec.S: the address of each vector's entry stub,
// which calls kerneltrap() through kernelvec.
extern uint64 kernelvecs[];

// in trampoline.S: the kernel address of each vector's entry
// stub, which calls usertrap() through uservec.
extern uint64 uservecs[];

int devintr(uint64 vector);
void setgate(struct gatedesc *g, uint64 entry, int dpl, int ist);

// what kernelvec.S leaves on the stack for kerneltrap():
// the registers it saves, the vector number and error code,
// and the frame the CPU pushed. riscv's kerneltrap() reads
// these from sepc, sstatus and scause instead.
struct kernelframe {
  uint64 rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11;
  uint64 vector;
  uint64 err;    // error code, or 0
  uint64 rip;    // riscv's sepc
  uint64 cs;     // previous privilege level is cs & 3
  uint64 rflags; // riscv's sstatus, for interrupts
  uint64 rsp;
  uint64 ss;
};

// the kernel's interrupt descriptor table, shared by all
// CPUs. riscv's stvec holds one trap address; x86 needs
// one gate per vector.
struct gatedesc kidt[256];

// riscv needs no segments, but x86 traps do: IDT gates name
// a code segment, and the CPU finds trap stacks in the task
// state segment (TSS). each CPU has its own TSS, so each CPU
// also needs its own GDT, with an entry that points to it.
// a trap from user space reads them with the user page table,
// so they are in cpuentry, which is mapped at CPUENTRY in
// every page table, like the trampoline.
__attribute__((aligned(PGSIZE))) struct cpuentry cpuentry;

// a separate stack for double faults. a double fault often
// means the kernel stack overflowed into its guard page, so
// the CPU could not push a trap frame there.
__attribute__((aligned(16))) char dfstack[NCPU][PGSIZE];

#define IST_DBLFLT 1 // tss.ist[] entry for dfstack

void
trapinit(void)
{
  initlock(&tickslock, "time");

  for (int i = 0; i < 256; i++)
    setgate(&kidt[i], kernelvecs[i], 0, 0);
  kidt[T_DBLFLT].ist = IST_DBLFLT;

  // the IDT for user mode, which prepare_return() loads.
  // its gates point at the stubs in the trampoline page,
  // at their addresses in user space. a double fault from
  // user mode needs no ist stack: the CPU switches to the
  // trapframe anyway (and dfstack is not in the user page
  // table). only the system call vector may be raised by
  // user code, with "int".
  for (int i = 0; i < 256; i++)
    setgate(&cpuentry.uidt[i], TRAMPOLINE + (uservecs[i] - (uint64)trampoline),
            i == T_SYSCALL ? 3 : 0, 0);
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  int id = cpuid();
  uint64 *g = cpuentry.gdt[id];
  struct tss *t = &cpuentry.tss[id];

  g[0] = 0;
  g[GDT_KCODE] = SEG_P | SEG_S | SEG_X | SEG_W | SEG_A | SEG_L | SEG_DPL(0);
  g[GDT_KDATA] = SEG_P | SEG_S | SEG_W | SEG_A | SEG_DPL(0);
  g[GDT_UDATA] = SEG_P | SEG_S | SEG_W | SEG_A | SEG_DPL(3);
  g[GDT_UCODE] = SEG_P | SEG_S | SEG_X | SEG_W | SEG_A | SEG_L | SEG_DPL(3);

  t->ist[IST_DBLFLT] = (uint64)dfstack[id] + PGSIZE;
  // a trap from user mode switches to rsp[0], the end of the
  // trapframe, so that the CPU saves the user's rip, rsp and
  // so on there, for uservec in trampoline.S. it is the same
  // address in every process (TRAPFRAME), so it is set here
  // once. it must be 16-byte aligned, as the CPU would align
  // it otherwise.
  t->rsp[0] = TRAPFRAME + sizeof(struct trapframe);
  // an I/O bitmap offset beyond the TSS means there is no
  // bitmap, so user code cannot use I/O ports.
  t->iomb = sizeof(struct tss);

  // the CPU finds the GDT and the TSS by virtual address,
  // in whatever page table is in use, so give it their
  // addresses at CPUENTRY rather than the kernel's own.

  // a TSS descriptor takes two GDT entries, to hold a
  // 64-bit base address.
  uint64 base = CPUENTRY_VA(t);
  uint64 limit = sizeof(struct tss) - 1;
  g[GDT_TSS] = (limit & 0xffff) | ((base & 0xffffff) << 16) | SEG_TSS64 |
               SEG_P | (((limit >> 16) & 0xf) << 48) |
               (((base >> 24) & 0xff) << 56);
  g[GDT_TSS + 1] = base >> 32;

  lgdt((void *)CPUENTRY_VA(g), sizeof(cpuentry.gdt[id]));
  loadsegs(KCODE_SEL, KDATA_SEL);
  ltr(TSS_SEL);

  // the x86 counterpart of riscv's w_stvec(kernelvec).
  lidt(kidt, sizeof(kidt));
}

// point IDT gate g at entry. dpl is the lowest privilege
// level allowed to reach it with an int instruction. ist
// selects a tss.ist[] stack, or 0 for the current stack.
void
setgate(struct gatedesc *g, uint64 entry, int dpl, int ist)
{
  g->off_15_0 = entry & 0xffff;
  g->cs = KCODE_SEL;
  g->ist = ist;
  g->type = GATE_P | GATE_DPL(dpl) | GATE_INTR;
  g->off_31_16 = (entry >> 16) & 0xffff;
  g->off_63_32 = entry >> 32;
  g->reserved = 0;
}

//
// handle an interrupt, exception, or system call from user space.
// called from, and returns to, trampoline.S
// return value is user cr3 for trampoline.S to switch to.
//
uint64
usertrap(void)
{
  int which_dev = 0;
  struct proc *p = myproc();

  // riscv reads the previous mode from sstatus; x86's is
  // the privilege level of the cs that the CPU saved.
  if ((p->trapframe->cs & 3) == 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  // the x86 counterpart of riscv's w_stvec(kernelvec).
  lidt(kidt, sizeof(kidt));

  // riscv saves the user program counter (sepc) here.
  // the CPU already saved it in p->trapframe->rip.

  if (p->trapframe->vector == T_SYSCALL) {
    // system call

    if (killed(p))
      kexit(-1);

    // riscv's sepc points to the ecall instruction, so it
    // adds 4 to return to the next one. the rip that "int"
    // saves already points to the next instruction.

    // the trap turned interrupts off; enable them only now
    // that we're done with the trap, as riscv does.
    intr_on();

    syscall();
  } else if ((which_dev = devintr(p->trapframe->vector)) != 0) {
    // ok
  } else if (p->trapframe->vector == T_PGFLT &&
             (p->trapframe->err & PFERR_I) == 0 &&
             vmfault(p->pagetable, p->sz, r_cr2(),
                     (p->trapframe->err & PFERR_W) ? 0 : 1) != 0) {
    // page fault on lazily-allocated page.
    // riscv's scause tells loads (13) from stores (15),
    // and an instruction fetch (12) is not handled;
    // x86 has one vector, and the error code tells them
    // apart.
  } else {
    printk("usertrap(): unexpected vector 0x%lx pid=%d\n",
           p->trapframe->vector, p->pid);
    printk("            rip=0x%lx err=0x%lx cr2=0x%lx\n", p->trapframe->rip,
           p->trapframe->err, r_cr2());
    setkilled(p);
  }

  if (killed(p))
    kexit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();

  prepare_return();

  // the user page table to switch to, for trampoline.S.
  // cr3 takes the page table's physical address as it is.
  uint64 cr3 = (uint64)p->pagetable;

  // return to trampoline.S; cr3 value in rax.
  return cr3;
}

//
// set up trapframe and control registers for a return to user space
//
void
prepare_return(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(). because a trap from kernel
  // code to usertrap would be a disaster, turn off interrupts.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S,
  // through the user IDT, at its address in user space.
  // the x86 counterpart of riscv's w_stvec(trampoline_uservec).
  lidt((void *)CPUENTRY_VA(cpuentry.uidt), sizeof(cpuentry.uidt));

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_cr3 = r_cr3();           // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;

  // set up the trapframe's rip, cs, rflags, rsp and ss,
  // which trampoline.S's iretq will use to get to user
  // space. riscv sets sstatus and sepc instead.

  // user code and stack segments, at privilege level 3
  // (riscv clears SPP for user mode).
  p->trapframe->cs = UCODE_SEL;
  p->trapframe->ss = UDATA_SEL;

  // enable interrupts in user mode.
  p->trapframe->rflags |= FL_IF;

  // rip and rsp already hold the saved user pc and stack
  // pointer (riscv writes sepc here).
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void
kerneltrap(struct kernelframe *f)
{
  int which_dev = 0;

  if ((f->cs & 3) != 0)
    panic("kerneltrap: not from kernel mode");
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr(f->vector)) == 0) {
    // interrupt or trap from an unknown source.
    // cr2 is riscv's stval, for page faults.
    printk("vector=0x%lx err=0x%lx rip=0x%lx cr2=0x%lx\n", f->vector, f->err,
           f->rip, r_cr2());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2 && myproc() != 0)
    yield();

  // riscv restores sepc and sstatus here, since the yield()
  // may have caused traps that changed them. x86 keeps them
  // in this trap's frame on the stack, which no other trap
  // writes, so iretq in kernelvec.S gets the right values.
}

void
clockintr()
{
  if (cpuid() == 0) {
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // ask for the next timer interrupt, a tenth of a second
  // from now (see lapicinit()). riscv's stimecmp write also
  // clears the interrupt request; the LAPIC instead needs
  // to be told that this interrupt is done.
  lapicsettimer(100000000);
  lapiceoi();
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr(uint64 vector)
{
  if (vector > T_TIMER && vector < T_SPURIOUS) {
    // this is a device interrupt, via the IOAPIC.

    // the vector tells which device interrupted
    // (see ioapicinit()); riscv asks the PLIC.
    int irq = vector - T_IRQ0;

    if (irq == UART0_IRQ) {
      uartintr();
    } else if (irq == VIRTIO0_IRQ) {
      virtio_disk_intr();
    } else {
      printk("unexpected interrupt irq=%d\n", irq);
    }

    // the LAPIC delivers no other interrupt of this
    // priority or lower until it is told that this
    // one is done.
    lapiceoi();

    return 1;
  } else if (vector == T_TIMER) {
    // timer interrupt.
    clockintr();
    return 2;
  } else if (vector == T_SPURIOUS) {
    // the LAPIC raises this when an interrupt went away
    // before the CPU took it. no interrupt is in
    // service, so there is nothing to tell the LAPIC.
    return 1;
  } else {
    return 0;
  }
}
