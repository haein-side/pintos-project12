#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	printf ("system call!\n");

	thread_exit ();
}

/* 주소값이 유저 영역에서 사용하는 주소 값인지 확인하는 함수
   (유저가상메모리 주소인지 아닌지)
   유저가상메모리 영역을 벗어났을 경우 프로세스 종료 (exit(-1)) */
void
check_address(void *addr) {
	struct thread *curr = thread_current ();
	// 짰던 코드 
	// 이렇게 짜면 안되는 이유: addr은 VA이기 때문에 PA인 USER_STACK과 LOADER_KERN_BASE와 비교하면 안 됨
	// if ((addr >= LOADER_KERN_BASE && addr < USER_STACK) || pml4_get_page(curr->pml4, addr) == NULL) { 
	// 	// 인자로 받은 주소값이 KERN_BASE보다 높은 값의 주소값을 가진 경우 or 매핑되지 않은 주소인 경우
	// 	fail ("invalid address");
	// }
	
	// 이렇게 하면 되려나? 나중에 테스트 해보기
	// if ((vtop(addr) >= LOADER_KERN_BASE && vtop(addr) < USER_STACK) || pml4_get_page(curr->pml4, addr) == NULL) { 
	// 	// 인자로 받은 주소값이 KERN_BASE보다 높은 값의 주소값을 가진 경우 or 매핑되지 않은 주소인 경우
	// 	fail ("invalid address");
	// }

	if (!is_user_vaddr(addr) || pml4_get_page(curr->pml4, addr) == NULL) { 
		// 인자로 받은 주소값이 KERN_BASE보다 높은 값의 주소값을 가진 경우 or 매핑되지 않은 주소인 경우
		exit(-1);
	}

}