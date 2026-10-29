# xv6-riscv와 x86-64 포팅의 대응

이 문서는 riscv 버전(`riscv` 브랜치)의 개념과 코드가 x86-64 포팅에서 무엇에
대응하는지 정리한 것이다. 코드에서 riscv와 다르게 간 곳에는 `riscv's ...`
형식의 주석으로 이유를 적어 두었으니, 함께 읽으면 된다.

## 1. 원칙

x86 관례대로 다시 짜지 않고, riscv 버전의 설계를 그대로 두고 아키텍처에
의존하는 부분만 x86-64 방식으로 바꾸었다. 그래서 다음은 riscv 버전과 같다.

- 커널 페이지 테이블과 유저 페이지 테이블이 따로 있다. 유저 주소 공간에는
  커널을 매핑하지 않는다.
- 유저와 커널 양쪽 주소 공간의 최상단 같은 주소에 trampoline 페이지가 있고,
  그 아래에 TRAPFRAME 페이지가 있다.
- `copyin`/`copyout`은 유저 페이지 테이블을 소프트웨어로 따라간다. 커널은
  유저 포인터를 직접 역참조하지 않는다.
- 트랩 흐름과 함수 이름이 같다: `uservec` → `usertrap()` →
  `prepare_return()` → `userret`, 커널 모드에서는 `kernelvec` →
  `kerneltrap()`.
- 커널 스택은 guard page와 함께 커널 페이지 테이블에 매핑한다
  (`proc_mapstacks`).
- 커널은 identity 매핑이다(VA = PA).
- 의미가 같으면 이름도 그대로 둔다. 예: `kvminithart()`, `trapinithart()`,
  부팅 메시지 `hart N starting`. 하드웨어가 다르면 이름만 바꾸고 호출 위치와
  역할은 유지했다: `riscv.h` → `x86.h`, `plic.c` → `ioapic.c`.

## 2. 부트

| riscv | x86-64 | 파일 |
|---|---|---|
| `qemu -machine virt -bios none -kernel`: 커널을 0x80000000에 올리고 모든 hart가 `_entry`에서 시작 | `qemu -machine q35 -kernel`: 커널을 0x100000에 올리고, PVH ELF note를 보고 첫 CPU만 `_entry`에서 시작(32bit 보호 모드, 페이징 꺼짐) | `entry.S` |
| `_entry`: `mhartid`로 CPU별 스택을 골라 `start()` 호출 | `_entry`: 임시 페이지 테이블(`bootpml4`, 1GB identity 2MB 페이지)로 페이징과 long mode를 켜고 64bit로 넘어간 뒤, `cpuid`의 LAPIC ID로 CPU별 스택을 골라 `start()` 호출 | `entry.S` |
| `start()`: machine mode에서 위임, PMP, 타이머를 설정하고 `mret`으로 supervisor mode의 `main()`으로 | `start()`: 이미 ring 0이라 할 일이 없다. GS base에 `struct cpu` 주소만 넣고 `main()` 호출 | `start.c` |
| riscv는 페이징 꺼진 채로 `main()` 시작(`satp = 0`) | long mode는 페이징 없이 돌 수 없어서 `entry.S`가 먼저 켜고, `kvminithart()`가 진짜 커널 페이지 테이블로 바꾼다 | `entry.S`, `vm.c` |

## 3. 메모리와 페이지 테이블

| riscv | x86-64 |
|---|---|
| Sv39: 3단계, 페이지 테이블 페이지당 PTE 512개 | 4단계 페이징: 4단계, 페이지당 PTE 512개 |
| `MAXVA` = 2^38 | `MAXVA` = 2^47 (최상위 비트를 쓰지 않아 sign extension을 피함, riscv와 같은 이유) |
| `satp` (`MAKE_SATP`, 모드 필드 필요) | `CR3` (페이지 테이블 주소만) |
| `sfence.vma` | `CR3` 쓰기가 TLB를 비움. 페이지 테이블 쓰기 전 fence 필요 없음 |
| PTE 비트 `V R W X U` = 비트 0~4 | `P`(=`PTE_V`) 0, `W` 1, `U` 2. x86에는 R, X 비트가 없어서 MMU가 무시하는 비트 9, 10에 `PTE_R`, `PTE_X`를 두고, `PTE_X`가 없으면 `PTE_NX`(63, `EFER.NXE` 필요)를 켠다 |
| PTE의 물리 주소는 10비트 밀린 위치 | 주소 자체와 같은 위치(비트 12~51), `PTE_ADDR`로 꺼냄 |
| 중간 단계 PTE는 R/W/X가 0이어야 함 | 중간 단계에도 W, U를 켠다. x86은 모든 단계에서 W와 U를 검사하기 때문 (`walk()`) |
| supervisor mode는 읽기 전용 PTE를 따름 | `CR0.WP`를 켜야 커널도 읽기 전용 PTE를 따름 (`entry.S`) |

### 주소 배치 (`memlayout.h`)

| 이름 | riscv | x86-64 | 비고 |
|---|---|---|---|
| `KERNBASE` | 0x80000000 | 0x100000 | PC의 처음 1MB는 BIOS 등이 씀 |
| `PHYSTOP` | `KERNBASE` + 128MB | 128MB | `-m 128M` |
| `UART0` | 0x10000000 (MMIO) | 0x3f8 (I/O 포트, COM1) | |
| 디스크 | `VIRTIO0` 0x10001000 (고정 MMIO) | PCI vendor 0x1af4, device 0x1042로 찾고 BAR 주소를 읽어 매핑 | `pci.c` |
| 인터럽트 컨트롤러 | `PLIC` 0x0c000000, `CLINT` 0x02000000 | `IOAPIC` 0xfec00000, `LAPIC` 0xfee00000 | |
| `TRAMPOLINE` | `MAXVA - PGSIZE` | 같음 | |
| `TRAPFRAME` | `TRAMPOLINE - PGSIZE` | 같음 | |
| `CPUENTRY` | 없음 | `TRAPFRAME - 2*PGSIZE` | 아래 설명 |
| `KSTACK(p)` | `TRAMPOLINE` 아래 | `CPUENTRY` 아래 | guard page 포함 2페이지씩 |
| `APENTRY` | 없음 | 0x8000 | 다른 CPU가 처음 실행하는 페이지 (8절) |
| 유저 메모리 상한 | `TRAPFRAME` 아래 | `CPUENTRY` 아래 | `sbrk`, `growproc` |

`CPUENTRY`: riscv는 트랩 진입 주소를 `stvec` 레지스터에서 읽지만, x86은 GDT,
TSS, IDT라는 메모리 위의 표를 가상 주소로 찾는다. 유저 모드에서 트랩이
오면 CPU는 유저 페이지 테이블로 이 표들을 읽으므로, trampoline처럼 유저와
커널 양쪽에 같은 주소로 매핑해야 한다. `CPUENTRY`의 두 페이지 중 하나에
CPU별 GDT와 TSS, 다른 하나에 유저용 IDT가 있다. `PTE_U`가 없어서 유저 코드는
읽을 수 없다.

커널 페이지 테이블(`kvmmake()`)에는 riscv의 UART, virtio, PLIC 대신 PCI BAR,
IOAPIC, LAPIC, `APENTRY`, 그리고 `CPUENTRY`가 더 들어간다.

## 4. 트랩

| riscv | x86-64 | 파일 |
|---|---|---|
| `stvec`: 모든 트랩이 가는 주소 하나 | IDT: 벡터 256개마다 게이트. 벡터별 스텁이 에러 코드 자리를 맞추고 벡터 번호를 push한 뒤 공통 코드로 점프 | `kernelvec.S`, `trampoline.S` |
| `w_stvec(kernelvec)` / `w_stvec(trampoline_uservec)` | IDT 두 개를 `lidt`로 바꿔 끼움: 커널용 `kidt`, 유저용 `cpuentry.uidt`(게이트가 trampoline 페이지 안의 스텁을 가리킴) | `trap.c` |
| `scause` | 벡터 번호(스텁이 push), 에러 코드 | `x86.h` `T_*` |
| `sepc` | `rip` (CPU가 push) | `trapframe`, `kernelframe` |
| `sstatus` (SPP, SIE, SPIE) | `cs & 3`(이전 권한), `rflags`의 `FL_IF` | |
| `stval` | `CR2` (page fault 주소) | |
| load/store page fault (`scause` 13/15, 명령어 fetch 12는 처리 안 함) | page fault 벡터 14 하나, 에러 코드의 W/I 비트로 구분 | `trap.c` |
| `ecall` (`scause` 8) | `int $0x40` (`T_SYSCALL`, DPL 3 게이트. 유저가 `int`로 부를 수 있는 유일한 벡터) | `usys.pl` |
| `epc += 4` | 필요 없음: `int`가 push하는 `rip`은 이미 다음 명령어 | `trap.c` |
| `sret` | `iretq` | `trampoline.S` |
| `sscratch`/`tp`로 trapframe과 hartid 찾기 | 유저 트랩이 오면 CPU가 TSS의 `rsp[0]`으로 스택을 바꾸는데, 이 값이 trapframe 끝(`TRAPFRAME + sizeof(struct trapframe)`)이라 CPU와 스텁의 push가 곧 trapframe 저장이 된다 | `trampoline.S`, `proc.h` |
| `tp` = hartid (`kernel_hartid`) | GS base = `struct cpu` 주소. 유저 진입/복귀 때 `swapgs` | `start.c`, `trampoline.S` |
| 세그먼트 없음 | CPU별 GDT(커널/유저 코드·데이터, TSS), TSS(`rsp[0]`, double fault용 `ist`) | `trap.c` `trapinithart()` |
| `trapframe`: 유저 레지스터 31개 + `kernel_satp`, `kernel_sp`, `kernel_trap`, `epc`, `kernel_hartid` | 유저 레지스터 15개 + `kernel_cr3`, `kernel_sp`, `kernel_trap`, 벡터, 에러 코드, CPU가 push하는 `rip`~`ss` | `proc.h` |

커널 모드 트랩은 riscv처럼 현재 커널 스택에서 처리한다. 커널 스택이 guard
page로 넘쳐 나는 double fault만 CPU별 별도 스택(`dfstack`, TSS `ist`)을 쓴다.

## 5. 인터럽트와 타이머

| riscv | x86-64 | 파일 |
|---|---|---|
| PLIC: 장치 인터럽트를 모아 hart에 전달 | IOAPIC: 입력(GSI) n을 벡터 `T_IRQ0 + n`으로 특정 CPU에 전달 | `ioapic.c` |
| `plic_claim()`으로 IRQ 번호 확인 | 벡터 번호에서 바로 계산 | `trap.c` `devintr()` |
| `plic_complete()` | LAPIC EOI | `lapic.c` |
| `plicinithart()`: hart별 enable, threshold 0 | `lapicinit()`: LAPIC 켜기, TPR 0 | `lapic.c` |
| UART 인터럽트를 모든 hart가 받음 | IOAPIC 항목은 CPU 하나만 지정 → CPU 0으로 보냄 | `ioapic.c` |
| 없음 | 8259 PIC: 모든 입력을 mask | `ioapic.c` |
| `stimecmp` (sstc): 다음 타이머 시각을 씀 | LAPIC 타이머 one-shot: `clockintr()`가 다음 카운트를 씀. 100000000 = 0.1초(QEMU), riscv의 1000000과 같은 간격 | `lapic.c`, `trap.c` |
| UART IRQ 10, virtio IRQ 1 | UART GSI 4, virtio GSI 23 (PCI 슬롯 3의 INTA, level-triggered) | `memlayout.h` |
| `wfi` | `sti; hlt; cli` (`hlt`는 인터럽트가 꺼져 있으면 깨지 않음) | `proc.c` `scheduler()` |
| `intr_on/off`: `sstatus.SIE` | `sti`/`cli`, `intr_get`: `rflags.IF` | `x86.h` |

## 6. 디스크

| riscv | x86-64 |
|---|---|
| virtio-mmio, 고정 주소 `VIRTIO0` | virtio-pci(modern, virtio 1.0). `pci.c`가 PCI 설정 공간(I/O 포트 0xcf8/0xcfc)에서 장치를 찾고, capability 목록에서 common/notify/ISR 영역과 BAR를 읽음 |
| 레지스터 오프셋은 MMIO 기준 | common configuration 구조의 오프셋. 큐 설정, 기능 협상 순서는 riscv 드라이버와 같음 |
| PLIC IRQ 1 | IOAPIC GSI 23, level-triggered. `virtio_disk_intr()`가 ISR 상태를 읽어야 내려감 |

## 7. 컨텍스트 스위치와 시스템 콜 ABI

| riscv | x86-64 | 파일 |
|---|---|---|
| `swtch`: `ra`, `sp`, `s0`~`s11` 저장 | `ra`(스택 위의 반환 주소), `sp`(반환 뒤의 값), callee-saved `rbx rbp r12~r15` 저장. 새 컨텍스트로는 `jmp *ra` | `swtch.S`, `proc.h` |
| 시스템 콜 번호 `a7`, 반환값 `a0` | 번호 `rax`, 반환값 `rax` | `usys.pl`, `syscall.c` |
| 인자 `a0`~`a5` | `rdi rsi rdx rcx r8 r9` (C 함수 호출 규약과 같아서 스텁이 옮길 필요 없음) | `syscall.c` `argraw()` |
| `exec`: argc는 시스템 콜 반환값(`a0`)이 곧 첫 인자, argv는 `a1` | 반환값은 `rax`라 `rdi` = argc, `rsi` = argv를 따로 설정. 유저 스택은 `main` 진입 때 정렬이 맞도록 sp − 8 | `exec.c` |
| 새 유저 이미지의 `sstatus.SPIE`를 `prepare_return()`이 매번 설정 | `rflags |= FL_IF` | `trap.c`, `exec.c` |

## 8. SMP

| riscv | x86-64 | 파일 |
|---|---|---|
| QEMU가 모든 hart를 `_entry`에서 시작 | 첫 CPU만 시작. 다른 CPU는 INIT IPI와 startup IPI를 기다림 | |
| 다른 hart는 `main()`에서 hart 0이 `started`를 켤 때까지 기다림 | 같음. 다만 그 전에 첫 CPU의 `main()`이 `lapicinit()` 뒤 `lapicstartothers()`로 `apentry`를 `APENTRY`(0x8000)에 복사하고 INIT + startup IPI 두 번을 "자기 말고 전부"에 보냄. 다른 CPU는 16bit real mode로 `apentry`에서 시작해 32bit 보호 모드로 바꾸고 `_entry`로 감. 이후 `_entry` → `start()` → `main()`은 첫 CPU와 같음 | `entry.S`, `lapic.c`, `main.c` |
| `cpuid()` = `tp`(hartid) | `start()`가 LAPIC ID 번째 `cpus[]`를 GS base에 넣으므로 `cpuid()`는 LAPIC ID. `mycpu()`는 GS base를 읽음 | `start.c`, `proc.c` |

## 9. 파일 대응

riscv 버전의 `kernel/` 파일은 모두 평평하게 한 폴더에 있다. 이 포팅에서는
역할별 하위 폴더로 나누었다(README의 "소스 구성"). 이름과 역할이 바뀐 것만
적는다.

| riscv | x86-64 | 비고 |
|---|---|---|
| `kernel/riscv.h` | `kernel/arch/x86/x86.h` | CSR 접근 → 레지스터, MSR, 포트 I/O, 디스크립터 |
| `kernel/plic.c` | `kernel/arch/x86/ioapic.c` | |
| 없음 | `kernel/arch/x86/lapic.c` | PLIC의 hart별 부분, 타이머, IPI |
| 없음 | `kernel/drivers/pci.c` | |
| `kernel/kernelvec.S` | `kernel/arch/x86/kernelvec.S` | 벡터 256개 스텁 + `kernelvec` |
| `kernel/trampoline.S` | `kernel/arch/x86/trampoline.S` | 벡터 256개 스텁 + `uservec`/`userret` |
| `kernel/entry.S` | `kernel/arch/x86/entry.S` | long mode 진입, PVH note, `apentry` |
| `.gdbinit.tmpl-riscv` | `.gdbinit.tmpl-x86_64` | |

## 10. 테스트에서 바꾼 것

- `usertests`의 잘못된 포인터 테스트(`copyin`, `copyout`, `copyinstr1`,
  `nowrite`, `lazy_copy` 등)는 riscv의 특정 주소(KERNBASE, 유저 최상단 페이지,
  TRAPFRAME, TRAMPOLINE, MAXVA)를 숫자로 쓴다. x86에서 같은 역할을 하는
  주소로 바꾸었다: 0x100000, 0x7fffffffc000(CPUENTRY), 0x7fffffffe000,
  0x7ffffffff000, 0x800000000000, 0x1000000000000.
- `lazy_sbrk`는 heap이 `CPUENTRY` 바로 아래까지 늘어난다고 본다.
- `uvmunmap()`은 leaf 페이지 테이블이 없으면 다음 2MB 경계로 건너뛴다.
  x86의 `MAXVA`가 riscv의 512배라, heap을 `MAXVA` 근처까지 lazy로 늘린 뒤
  `exit`하면 페이지마다 `walk()`하느라 수십 분이 걸렸기 때문이다. 건너뛰면
  반복 횟수가 riscv와 같아진다.
