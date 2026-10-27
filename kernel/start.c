#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "x86.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

void main();

// entry.S needs one stack per CPU.
__attribute__((aligned(16))) char stack0[4096 * NCPU];

// entry.S jumps here in 64-bit mode on stack0.
//
// on riscv, start() runs in machine mode and must prepare
// supervisor mode (previous privilege, trap delegation,
// physical memory protection, A/D bit updates, timer)
// before mret switches to supervisor mode in main().
// x86 has no such separate mode: entry.S already left
// the CPU in kernel mode (ring 0), every trap goes to
// the kernel's IDT without delegation, and the MMU
// always updates PTE A/D bits. so none of that is needed.
void
start()
{
  // keep a pointer to each CPU's struct cpu in its GS base
  // register, for mycpu(). riscv keeps the hartid in tp
  // instead; x86 has no spare register for it, but reading
  // through gs is one instruction.
  struct cpu *c = &cpus[r_apicid()];
  c->self = c;
  wrmsr(MSR_GS_BASE, (uint64)c);

  main();
}
