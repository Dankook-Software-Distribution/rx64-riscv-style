// Saved registers for kernel context switches.
struct context {
  uint64 ra; // where swtch() resumes (x86's call keeps it on the stack)
  uint64 sp;

  // callee-saved
  uint64 rbx;
  uint64 rbp;
  uint64 r12;
  uint64 r13;
  uint64 r14;
  uint64 r15;
};

// Per-CPU state.
struct cpu {
  struct cpu *self;       // This struct, at the GS base (see start.c).
  struct proc *proc;      // The process running on this cpu, or null.
  struct context context; // swtch() here to enter scheduler().
  int noff;               // Depth of push_off() nesting.
  int intena;             // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// the x86 descriptor tables that traps from user space need,
// in the two pages mapped at CPUENTRY in both user and kernel
// space (see memlayout.h): each CPU's GDT and TSS in the
// first page, the IDT for user mode in the second.
struct cpuentry {
  uint64 gdt[NCPU][NGDT];
  struct tss tss[NCPU];
  struct gatedesc uidt[256] __attribute__((aligned(PGSIZE)));
};

extern struct cpuentry cpuentry;

// the address at CPUENTRY of x, which is in cpuentry.
#define CPUENTRY_VA(x) (CPUENTRY + ((uint64)(x) - (uint64)&cpuentry))

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// a trap from user space switches to the stack that ends at the
// end of the trapframe (the TSS's rsp[0], see trapinithart()):
// the CPU pushes ss through rip, the vector's entry stub pushes
// the vector and error code, then uservec in trampoline.S pushes
// the user registers, initializes registers from the trapframe's
// kernel_sp and kernel_cr3, and jumps to kernel_trap.
// prepare_return() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// riscv's kernel_hartid is not needed: swapgs in trampoline.S
// switches gs between the user's and this CPU's struct cpu.
struct trapframe {
  /*   0 */ uint64 kernel_cr3;  // kernel page table
  /*   8 */ uint64 kernel_sp;   // top of process's kernel stack
  /*  16 */ uint64 kernel_trap; // usertrap()
  /*  24 */ uint64 pad;         // keeps the end 16-byte aligned

  // pushed by uservec
  /*  32 */ uint64 r15;
  /*  40 */ uint64 r14;
  /*  48 */ uint64 r13;
  /*  56 */ uint64 r12;
  /*  64 */ uint64 r11;
  /*  72 */ uint64 r10;
  /*  80 */ uint64 r9;
  /*  88 */ uint64 r8;
  /*  96 */ uint64 rbp;
  /* 104 */ uint64 rdi;
  /* 112 */ uint64 rsi;
  /* 120 */ uint64 rdx;
  /* 128 */ uint64 rcx;
  /* 136 */ uint64 rbx;
  /* 144 */ uint64 rax;

  // pushed by the vector's entry stub
  /* 152 */ uint64 vector; // riscv's scause
  /* 160 */ uint64 err;    // error code, or 0

  // pushed by the CPU
  /* 168 */ uint64 rip; // saved user program counter (riscv's epc)
  /* 176 */ uint64 cs;
  /* 184 */ uint64 rflags;
  /* 192 */ uint64 rsp;
  /* 200 */ uint64 ss;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state; // Process state
  void *chan;           // If non-zero, sleeping on chan
  int killed;           // If non-zero, have been killed
  int xstate;           // Exit status to be returned to parent's wait
  int pid;              // Process ID

  // wait_lock must be held when using this:
  struct proc *parent; // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};
