#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "x86.h"
#include "defs.h"

//
// PCI configuration space. riscv's qemu virt machine puts
// the virtio disk at a fixed address, but on a PC the disk
// is a PCI device: the kernel finds it by its vendor and
// device IDs, and the firmware has chosen the addresses of
// its registers, which the kernel reads from its BARs
// (base address registers).
//

// each PCI function has 256 bytes of configuration space,
// reached through two I/O ports: write the function's
// address and a register offset to CONFIG_ADDRESS, then
// read or write the register through CONFIG_DATA.
#define CONFIG_ADDRESS 0xcf8
#define CONFIG_DATA    0xcfc

// configuration space registers.
#define PCI_VENDOR  0x00 // 16 bits; 0xffff if no device
#define PCI_DEVICE  0x02 // 16 bits
#define PCI_COMMAND 0x04 // 16 bits
#define PCI_STATUS  0x06 // 16 bits
#define PCI_BAR0    0x10 // six 32-bit BARs
#define PCI_CAPPTR  0x34 // 8 bits: offset of the first capability

#define COMMAND_MEMORY  0x0002 // respond to memory addresses (BARs)
#define COMMAND_MASTER  0x0004 // may read and write RAM (DMA)
#define COMMAND_INTXOFF 0x0400 // INTx interrupts disabled

#define STATUS_CAPLIST 0x0010 // has a capability list

#define BAR_IO     0x1 // an I/O port BAR, not memory
#define BAR_TYPE   0x6 // memory BAR type:
#define BAR_TYPE64 0x4 //   64 bits wide, using the next BAR too
#define BAR_ADDR   (~0xfUL)

// a function's address: bus, device (slot), and function
// numbers, as CONFIG_ADDRESS wants them (bits 23-8).
#define PCI_DEV(bus, slot, func) (((bus) << 16) | ((slot) << 11) | ((func) << 8))

// select register off (rounded down to 4 bytes) of dev.
static void
pci_select(int dev, int off)
{
  outl(CONFIG_ADDRESS, 0x80000000 | dev | (off & 0xfc));
}

uint32
pci_read32(int dev, int off)
{
  pci_select(dev, off);
  return inl(CONFIG_DATA);
}

uint16
pci_read16(int dev, int off)
{
  pci_select(dev, off);
  return inw(CONFIG_DATA + (off & 2));
}

uchar
pci_read8(int dev, int off)
{
  pci_select(dev, off);
  return inb(CONFIG_DATA + (off & 3));
}

void
pci_write32(int dev, int off, uint32 x)
{
  pci_select(dev, off);
  outl(CONFIG_DATA, x);
}

void
pci_write16(int dev, int off, uint16 x)
{
  pci_select(dev, off);
  outw(CONFIG_DATA + (off & 2), x);
}

// find the PCI function with the given vendor and device
// IDs, and return its address, or -1 if there is none.
// qemu puts its devices on bus 0, as function 0 of their
// slots, so look only there.
int
pci_find(uint16 vendor, uint16 device)
{
  for (int slot = 0; slot < 32; slot++) {
    int dev = PCI_DEV(0, slot, 0);
    if (pci_read16(dev, PCI_VENDOR) == vendor &&
        pci_read16(dev, PCI_DEVICE) == device)
      return dev;
  }
  return -1;
}

// let dev respond to its memory BARs and use DMA, and let
// it interrupt through its INTx pin (the IOAPIC).
void
pci_enable(int dev)
{
  uint16 cmd = pci_read16(dev, PCI_COMMAND);
  cmd |= COMMAND_MEMORY | COMMAND_MASTER;
  cmd &= ~COMMAND_INTXOFF;
  pci_write16(dev, PCI_COMMAND, cmd);
}

// return the address of dev's memory BAR n, and set *size
// to its size; return 0 if BAR n is unused, is an I/O
// port BAR, or is the upper half of a 64-bit BAR n-1.
uint64
pci_bar(int dev, int n, uint64 *size)
{
  int off = PCI_BAR0 + 4 * n;
  if (n > 0) {
    uint32 prev = pci_read32(dev, off - 4);
    if (!(prev & BAR_IO) && (prev & BAR_TYPE) == BAR_TYPE64)
      return 0;
  }
  uint32 lo = pci_read32(dev, off);
  if (lo & BAR_IO)
    return 0;
  int is64 = (lo & BAR_TYPE) == BAR_TYPE64;
  uint32 hi = is64 ? pci_read32(dev, off + 4) : 0;

  // the size: write all 1s, and the BAR reads back with
  // the bits below its size as 0 (all of it 0 if unused).
  // turn off the memory addresses while the BAR holds
  // this bogus address.
  uint16 cmd = pci_read16(dev, PCI_COMMAND);
  pci_write16(dev, PCI_COMMAND, cmd & ~COMMAND_MEMORY);
  pci_write32(dev, off, 0xffffffff);
  uint64 mask = pci_read32(dev, off) & BAR_ADDR;
  pci_write32(dev, off, lo);
  if (is64) {
    pci_write32(dev, off + 4, 0xffffffff);
    mask |= (uint64)pci_read32(dev, off + 4) << 32;
    pci_write32(dev, off + 4, hi);
  } else if (mask != 0) {
    mask |= 0xffffffff00000000UL;
  }
  pci_write16(dev, PCI_COMMAND, cmd);

  if (mask == 0)
    return 0;
  *size = ~mask + 1;
  return (((uint64)hi << 32) | lo) & BAR_ADDR;
}

// return the offset of dev's first capability with ID id
// after the one at offset off (0 to start at the first),
// or 0 if there are no more. each capability starts with
// its ID and the offset of the next one, whose low 2 bits
// are reserved.
int
pci_nextcap(int dev, int off, int id)
{
  if (off == 0) {
    if (!(pci_read16(dev, PCI_STATUS) & STATUS_CAPLIST))
      return 0;
    off = pci_read8(dev, PCI_CAPPTR) & 0xfc;
  } else {
    off = pci_read8(dev, off + 1) & 0xfc;
  }
  for (; off != 0; off = pci_read8(dev, off + 1) & 0xfc) {
    if (pci_read8(dev, off) == id)
      return off;
  }
  return 0;
}
