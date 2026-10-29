OUTPUT_FORMAT( "elf64-x86-64" )
OUTPUT_ARCH( "i386:x86-64" )
ENTRY( _entry )

SECTIONS
{
  /*
   * load the kernel at 0x100000 (1MB), the start of
   * the RAM above the PC's legacy BIOS and VGA area.
   * qemu's -kernel jumps to _entry, whose address
   * entry.S records in a PVH ELF note.
   */
  . = 0x100000;

  .text : {
    kernel/arch/x86/entry.o(_entry)
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    PROVIDE(etext = .);
  }

  .rodata : {
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  /*
   * the PVH note tells qemu's -kernel where _entry is.
   * it must be in a loaded section so that the
   * linker puts it in a PT_NOTE program header.
   */
  .note : {
    *(.note.Xen)
  }

  .data : {
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  PROVIDE(end = .);
}
