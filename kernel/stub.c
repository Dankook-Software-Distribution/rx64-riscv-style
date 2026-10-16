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

struct proc *myproc(void) { NOT_PORTED("myproc"); }
void procinit(void) { NOT_PORTED("procinit"); }
void userinit(void) { NOT_PORTED("userinit"); }
void scheduler(void) { NOT_PORTED("scheduler"); }
void sleep_prepare(void *chan) { NOT_PORTED("sleep_prepare"); }
void sleep(void) { NOT_PORTED("sleep"); }
void wakeup(void *chan) { NOT_PORTED("wakeup"); }
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

// vm.c

void kvminit(void) { NOT_PORTED("kvminit"); }
void kvminithart(void) { NOT_PORTED("kvminithart"); }
uint64 walkaddr(pagetable_t pagetable, uint64 va) { NOT_PORTED("walkaddr"); }
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm) { NOT_PORTED("uvmalloc"); }
void uvmclear(pagetable_t pagetable, uint64 va) { NOT_PORTED("uvmclear"); }
int copyout(pagetable_t pagetable, uint64 dstva, uint64 src, char *s, uint64 len) { NOT_PORTED("copyout"); }
int copyin(pagetable_t pagetable, uint64 dst, char *d, uint64 srcva, uint64 len) { NOT_PORTED("copyin"); }
int copyinstr(pagetable_t pagetable, uint64 dst, char *d, uint64 srcva, uint64 max) { NOT_PORTED("copyinstr"); }

// trap.c

struct spinlock tickslock;
uint ticks;

void trapinit(void) { NOT_PORTED("trapinit"); }
void trapinithart(void) { NOT_PORTED("trapinithart"); }

// virtio_disk.c

void virtio_disk_init(void) { NOT_PORTED("virtio_disk_init"); }
void virtio_disk_rw(struct buf *b, int write) { NOT_PORTED("virtio_disk_rw"); }

// trampoline.S

// kernel.ld requires the trampoline section to be exactly one page.
__attribute__((section("trampsec"), aligned(PGSIZE))) const char trampoline[PGSIZE];
