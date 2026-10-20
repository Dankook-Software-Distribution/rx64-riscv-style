//
// temporary stand-ins for kernel files that have not been
// ported to x86-64 yet, so that the rest of the kernel can
// be linked and booted while the port proceeds file by file.
//
// each group below replaces one file that the Makefile leaves
// out of OBJS. when that file is ported, put it back in OBJS
// and delete its group here. delete this file when it is empty.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "x86.h"
#include "proc.h"
#include "defs.h"

#define NOT_PORTED(name) panic(name ": not ported yet")

// proc.c

struct cpu cpus[NCPU];

// only CPU 0 runs until the other CPUs are started,
// so these two work without per-CPU registers.
int
cpuid()
{
  return 0;
}

struct cpu *
mycpu(void)
{
  return &cpus[0];
}

// until proc.c is ported there are no processes, so
// main() boots all the way to the scheduler, which can
// only wait for interrupts. these behave as proc.c's do
// with no processes.

// main() calls these while booting, so they cannot panic.
void procinit(void) { }
void userinit(void) { }

// there is never a current process.
struct proc *myproc(void) { return 0; }

// interrupt handlers call this (clockintr(), uartintr(),
// consoleintr()); with no processes, no one is waiting.
void wakeup(void *chan) { }

// the part of riscv's scheduler() that runs when no
// process is RUNNABLE: wait for an interrupt. hlt, unlike
// riscv's wfi, does not wake up while interrupts are off,
// so turn them on just for it. sti takes effect only after
// the next instruction, so an interrupt cannot be handled
// between sti and hlt and leave the CPU halted until the
// interrupt after it.
void
scheduler(void)
{
  for (;;)
    asm volatile("sti; hlt; cli");
}

void yield(void) { NOT_PORTED("yield"); }
void sleep_prepare(void *chan) { NOT_PORTED("sleep_prepare"); }
void sleep(void) { NOT_PORTED("sleep"); }
int kfork(void) { NOT_PORTED("kfork"); }
void kexit(int status) { NOT_PORTED("kexit"); }
int kwait(uint64 addr) { NOT_PORTED("kwait"); }
int kkill(int pid) { NOT_PORTED("kkill"); }
int killed(struct proc *p) { NOT_PORTED("killed"); }
int growproc(int n) { NOT_PORTED("growproc"); }
pagetable_t proc_pagetable(struct proc *p) { NOT_PORTED("proc_pagetable"); }
void proc_freepagetable(pagetable_t pagetable, uint64 sz) { NOT_PORTED("proc_freepagetable"); }
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len) { NOT_PORTED("either_copyout"); }
int either_copyin(void *dst, int user_src, uint64 src, uint64 len) { NOT_PORTED("either_copyin"); }
void procdump(void) { NOT_PORTED("procdump"); }

// kvmmake() calls this while booting, so it cannot panic.
// it maps no kernel stacks until proc.c is ported.
void proc_mapstacks(pagetable_t kpgtbl) { }

// virtio_disk.c

// main() calls this while booting, so it cannot panic.
// nothing reads the disk while there are no processes.
void virtio_disk_init(void) { }
void virtio_disk_rw(struct buf *b, int write) { NOT_PORTED("virtio_disk_rw"); }
void virtio_disk_intr(void) { NOT_PORTED("virtio_disk_intr"); }

// trampoline.S

// kernel.ld requires the trampoline section to be exactly one page.
__attribute__((section("trampsec"), aligned(PGSIZE))) const char trampoline[PGSIZE];
