#include "threads/loader.h"

.text
.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
	movq %rbx, temp1(%rip)	   /* rbx, r12값을 임시로 저장 */
	movq %r12, temp2(%rip)     /* callee saved registers */
	movq %rsp, %rbx            /* rsp를 rbx에 값을 옮겨둠, Store userland rsp    */
	movabs $tss, %r12		   /* 쓰레드를 생성할 때 저장해 두었던 커널 스택 공간을 가리키는 rsp값을 꺼내서 (tss에 들어있음) */
	movq (%r12), %r12		   /* 유저의 rsp에 덮어쓴다. 이때부터 커널 스택 공간으로 들어감 */
	movq 4(%r12), %rsp         /* tss 구조체에서 rsp0를 가리키기 위해 4만큼을 더해준다 ,Read ring0 rsp from the tss */
	/* Now we are in the kernel stack */
	/* 커널 스택에 아래의 레지스터 값들을 쌓아준다. */
	push $(SEL_UDSEG)      /* if->ss */
	push %rbx              /* if->rsp */
	push %r11              /* if->eflags */
	push $(SEL_UCSEG)      /* if->cs */
	push %rcx              /* if->rip */
	subq $16, %rsp         /* skip error_code, vec_no */
	push $(SEL_UDSEG)      /* if->ds */
	push $(SEL_UDSEG)      /* if->es */
	push %rax
	movq temp1(%rip), %rbx
	push %rbx
	pushq $0
	push %rdx
	push %rbp
	push %rdi
	push %rsi
	push %r8
	push %r9
	push %r10
	pushq $0 /* skip r11 */
	movq temp2(%rip), %r12
	push %r12
	push %r13
	push %r14
	push %r15
	movq %rsp, %rdi

check_intr:
	btsq $9, %r11          /* Check whether we recover the interrupt */
	jnb no_sti
	sti                    /* restore interrupt */
no_sti:
	movabs $syscall_handler, %r12 // 명령어의 주소를 %r12 레지스터에 넣어준다.
	call *%r12					  // syscall_handler를 실제로 호출하는 부분, call의 역할은 2가지. 옆의 명령어를 실행하는 것과 다음에 수행할 명령줄의 주소를 스택에 저장해준다.
	popq %r15					  // syscall_handler 명령이 끝나면 여기로 돌아온다.
	popq %r14					  // 인터럽트 프레임에 해당 레지스터들을 담아줌
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rsi
	popq %rdi
	popq %rbp
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax
	addq $32, %rsp
	popq %rcx              /* if->rip */
	addq $8, %rsp
	popq %r11              /* if->eflags */
	popq %rsp              /* if->rsp */
	sysretq				   /* 인터럽트 핸들러 종료 후, 원래의 프로그램으로 복귀시켜주는 명령어(유저모드로)*/

.section .data
.globl temp1
temp1:
.quad	0
.globl temp2
temp2:
.quad	0
