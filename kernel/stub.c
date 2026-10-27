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

// trampoline.S

// kernel.ld requires the trampoline section to be exactly one page.
__attribute__((section("trampsec"), aligned(PGSIZE))) const char trampoline[PGSIZE];

// forkret() in proc.c jumps to userret in the trampoline page.
// prepare_return() panics before forkret() gets there, so any
// address in the page will do until trampoline.S is ported.
asm(".globl userret\n.set userret, trampoline");
