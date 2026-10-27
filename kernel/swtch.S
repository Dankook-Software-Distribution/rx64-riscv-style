# Context switch
#
#   void swtch(struct context *old, struct context *new);
#
# Save current registers in old. Load from new.
#
# riscv's call leaves the return address in ra, but x86's
# leaves it on top of the stack. so save it as old->ra, and
# save sp as it will be once swtch returns; then resume new
# by jumping to new->ra with new->sp, as if swtch had
# returned there.
.globl swtch
swtch:
        movq 0(%rsp), %rax
        movq %rax, 0(%rdi)
        leaq 8(%rsp), %rax
        movq %rax, 8(%rdi)
        movq %rbx, 16(%rdi)
        movq %rbp, 24(%rdi)
        movq %r12, 32(%rdi)
        movq %r13, 40(%rdi)
        movq %r14, 48(%rdi)
        movq %r15, 56(%rdi)

        movq 0(%rsi), %rax
        movq 8(%rsi), %rsp
        movq 16(%rsi), %rbx
        movq 24(%rsi), %rbp
        movq 32(%rsi), %r12
        movq 40(%rsi), %r13
        movq 48(%rsi), %r14
        movq 56(%rsi), %r15
        jmp *%rax
