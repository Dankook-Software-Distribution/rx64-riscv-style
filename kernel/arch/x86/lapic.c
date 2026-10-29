#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "x86.h"
#include "defs.h"

//
// the x86 local APIC (LAPIC), one per CPU. it delivers
// interrupts to its own CPU, and it has the timer, so it
// does the per-hart work of riscv's PLIC (threshold,
// complete) as well as riscv's stimecmp timer.
//

// LAPIC registers, as byte offsets from LAPIC.
// each CPU sees its own LAPIC at the same address.
#define LAPIC_TPR   0x080 // task priority
#define LAPIC_ICRLO 0x300 // interrupt command (IPIs), low half
#define LAPIC_EOI   0x0b0 // end of interrupt
#define LAPIC_SVR   0x0f0 // spurious interrupt vector
#define LAPIC_TIMER 0x320 // local vector table (LVT): timer
#define LAPIC_LINT0 0x350 // LVT: local interrupt pin 0
#define LAPIC_LINT1 0x360 // LVT: local interrupt pin 1
#define LAPIC_ERROR 0x370 // LVT: error
#define LAPIC_TICR  0x380 // timer initial count
#define LAPIC_TDCR  0x3e0 // timer divide configuration

#define SVR_ENABLE 0x100   // turn the LAPIC on
#define LVT_MASKED 0x10000 // the interrupt is masked
#define TDCR_DIV1  0xb     // timer counts at its full rate

#define ICR_INIT    0x00500 // INIT IPI: reset the CPU
#define ICR_STARTUP 0x00600 // startup IPI: run at vector << 12
#define ICR_DELIVS  0x01000 // the IPI is still being sent
#define ICR_ASSERT  0x04000 // level: assert
#define ICR_OTHERS  0xc0000 // to all CPUs except this one

#define Reg(reg) ((volatile uint32 *)(LAPIC + (reg)))

// set up this CPU's LAPIC. called on each CPU.
void
lapicinit(void)
{
  // turn the LAPIC on, and pick the vector it raises for a
  // spurious interrupt (one that went away before the CPU
  // took it).
  *Reg(LAPIC_SVR) = SVR_ENABLE | T_SPURIOUS;

  // the firmware wires LINT0 to the 8259 and LINT1 to NMI.
  // xv6 masks the 8259 and does not use NMIs. riscv has no
  // LAPIC error interrupt, so xv6 does not use that either.
  *Reg(LAPIC_LINT0) = LVT_MASKED;
  *Reg(LAPIC_LINT1) = LVT_MASKED;
  *Reg(LAPIC_ERROR) = LVT_MASKED;

  // take interrupts of any priority, as riscv's
  // plicinithart() sets this hart's threshold to 0.
  *Reg(LAPIC_TPR) = 0;

  // finish any interrupt the firmware left in service.
  lapiceoi();

  // riscv's start.c asks for the first timer interrupt.
  // the timer counts down once (one-shot) and interrupts at
  // zero; clockintr() starts the next count, as riscv's
  // clockintr() sets the next stimecmp. qemu's LAPIC timer
  // counts once per nanosecond at divide 1, so 100000000
  // is a tenth of a second, the same as riscv's 1000000.
  *Reg(LAPIC_TDCR) = TDCR_DIV1;
  *Reg(LAPIC_TIMER) = T_TIMER;
  lapicsettimer(100000000);
}

// start the timer counting down from count; it interrupts
// when it reaches zero.
void
lapicsettimer(uint32 count)
{
  *Reg(LAPIC_TICR) = count;
}

// tell the LAPIC we've served the current interrupt,
// as riscv's plic_complete() tells the PLIC.
void
lapiceoi(void)
{
  *Reg(LAPIC_EOI) = 0;
}

static void
lapicipi(uint32 icr)
{
  *Reg(LAPIC_ICRLO) = icr;
  while (*Reg(LAPIC_ICRLO) & ICR_DELIVS)
    ;
}

extern char apentry[], apentryend[]; // entry.S

// start the other CPUs at apentry in entry.S, which takes
// them to _entry. qemu starts every riscv hart at _entry,
// but x86 CPUs other than the first wait for an INIT IPI
// and then a startup IPI. sending them to "all except this
// one" needs no list of the CPUs. real hardware wants
// delays between the IPIs, but qemu does not; the second
// startup IPI is for hardware that misses the first.
void
lapicstartothers(void)
{
  memmove((void *)APENTRY, apentry, apentryend - apentry);

  lapicipi(ICR_OTHERS | ICR_INIT | ICR_ASSERT);
  for (int i = 0; i < 2; i++)
    lapicipi(ICR_OTHERS | ICR_STARTUP | ICR_ASSERT | (APENTRY >> 12));
}
