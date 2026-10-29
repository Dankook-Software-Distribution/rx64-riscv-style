#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "x86.h"
#include "defs.h"

//
// the x86 I/O APIC (IOAPIC), riscv's PLIC: it takes
// interrupts from devices and sends each one to a CPU's
// LAPIC (see lapic.c) as a vector.
//

// the IOAPIC has just two memory-mapped registers: write
// the number of an internal register to IOREGSEL, then
// read or write that register through IOWIN.
#define IOREGSEL (IOAPIC + 0x00)
#define IOWIN    (IOAPIC + 0x10)

// internal registers.
#define REG_VER   0x01 // version, and number of inputs
#define REG_TABLE 0x10 // redirection table: two registers
                       // (low, high) for each input (GSI)

// redirection table entry, low register. an entry
// with all of these clear is edge-triggered and active-high,
// and goes to one CPU named by its LAPIC ID (high register).
#define INT_LEVEL  0x00008000 // level-triggered (else edge)
#define INT_ACTLOW 0x00002000 // active low (else high)
#define INT_MASKED 0x00010000 // interrupt masked

// the PC's older interrupt controllers, the two 8259 PICs,
// have their mask registers at these I/O ports.
#define PIC1_DATA 0x21
#define PIC2_DATA 0xa1

static uint32
ioapicread(int reg)
{
  *(volatile uint32 *)IOREGSEL = reg;
  return *(volatile uint32 *)IOWIN;
}

static void
ioapicwrite(int reg, uint32 data)
{
  *(volatile uint32 *)IOREGSEL = reg;
  *(volatile uint32 *)IOWIN = data;
}

// send input irq to the CPU with LAPIC ID apicid,
// as vector T_IRQ0 + irq.
static void
ioapicroute(int irq, uint32 flags, int apicid)
{
  ioapicwrite(REG_TABLE + 2 * irq + 1, (uint32)apicid << 24);
  ioapicwrite(REG_TABLE + 2 * irq, flags | (T_IRQ0 + irq));
}

void
ioapicinit(void)
{
  // riscv has nothing like the 8259s. they can still raise
  // interrupts through LINT0 (see lapic.c), so mask all of
  // their inputs, and let devices interrupt only through
  // the IOAPIC.
  outb(PIC1_DATA, 0xff);
  outb(PIC2_DATA, 0xff);

  // an IOAPIC input with no device must not interrupt, as a
  // PLIC IRQ with priority 0 does not. mask them all, then
  // unmask the ones xv6 uses.
  int maxintr = (ioapicread(REG_VER) >> 16) & 0xff;
  for (int irq = 0; irq <= maxintr; irq++)
    ioapicwrite(REG_TABLE + 2 * irq, INT_MASKED);

  // riscv's plicinithart() lets every hart take the uart's
  // interrupts, but an IOAPIC entry names one CPU, so send
  // them to CPU 0 (LAPIC ID 0). the uart's interrupt is
  // edge-triggered and active-high.
  ioapicroute(UART0_IRQ, 0, 0);

  // the virtio disk's PCI INTA pin is level-triggered and
  // (as the ACPI tables say) active-high: it stays raised
  // until virtio_disk_intr() reads the device's ISR status.
  ioapicroute(VIRTIO0_IRQ, INT_LEVEL, 0);
}
