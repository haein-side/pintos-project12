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
syscall_handler (struct intr_frame *f UNUSED) 
{
	/* 유저 스택에 저장되어 있는 시스템 콜 넘버를 가져오기 */	
	int sys_number = f->R.rax; // rax : 시스템 콜 넘버

	/*
		인자가 들어오는 순서 : 
		1번째 인자 : %rdi
		2번째 인자 : %rsi
		3번째 인자 : %rdx
		4번째 인자 : %r10
		5번째 인자 : %r8
		6번째 인자 : %r9
	*/
	// TODO: Your implementation goes here.
	switch (sys_number) {
		case SYS_HALT : halt();
		case SYS_EXIT : exit(f->R.rdi);
		case SYS_FORK : fork(f->R.rdi);
		case SYS_EXEC : exec(f->R.rdi);
		case SYS_CREATE : create(f->R.rdi, f->R.rsi);
		case SYS_REMOVE : remove(f->R.rdi);
		case SYS_OPEN : open(f->R.rdi);
		case SYS_FILESIZE : filesize(f->R.rdi);
		case SYS_READ : read(f->R.rdi, f->R.rsi, f->R.rdx);
		case SYS_WRITE : write(f->R.rdi, f->R.rsi, f->R.rdx);
		case SYS_SEEK : seek(f->R.rdi, f->R.rdx);
		case SYS_TELL : tell(f->R.rdi);
		case SYS_CLOSE : close(f->R.rdi);


	}
	printf ("system call!\n");
	thread_exit ();
}

/* 해당 주소값이 유저 가상 주소인지 체크. 유저 영역이 아니라면 종료하는 함수 */
void check_address(void *addr)
{
	struct thread *t = thread_current();
	/* is_user_vaddr(addr) : 해당 주소가 유저 가상 주소에 해당하는지 체크
	 * addr == NULL : 주소가 비어있는지 체크 
	 * pml4_get_page(t->pml4, addr) == NULL : 포인터가 가리키는 주소가 유저 영역 내에 있지만, 페이지로 할당하지 않은 영역일 수도 있으므로 체크*/
	if (!is_user_vaddr(addr) || addr == NULL || pml4_get_page(t->pml4, addr) == NULL){
		exit(-1);
	}
}

/* pintos를 종료시키는 시스템콜 */
void halt(void) 
{
	power_off();
}

/* 현재 돌고 있는 프로세스만 종료시키는 시스템콜 */
void exit (int status)
{
	struct thread *t = thread_current();
	t->exit_status = status;
	printf("%s: exit%d\n", t->name, status); // Process Termination Message
	/* 정상적으로 종료되었다면 status = 0 */
	/* status : 프로그램이 정상적으로 종료됐는지 확인 */
	thread_exit();
}
/* 파일을 생성하는 시스템콜 */
bool create (const char *file, unsigned initial_size)
{	
	/* 성공이면 true, 실패면 false */
	check_address(file);
	if (filesys_create(file, initial_size)) {
		return true;
	}
	else {
		return false;
	}
}

/* 파일을 제거하는 시스템콜 */
bool remove (const char *file)
{
	/* 파일을 제거하더라도, 그 이전에 파일을 오픈했다면 해당 오픈 파일은 close되지 않고,
	 * 켜진 상태로 남아있는다. */
	check_address(file);
	if (filesys_remove(file)) {
		return true;
	}
	else {
		return false;
	}
}

int write (int fd, const void *buf, unsigned size) 
{
	if (fd == STDOUT_FILENO)
		putbuf(buf, size);
	return size;
}

/* 사용자 프로세스가 파일에 접근하기 위해 요청하는 시스템 콜 */
int open (const char *file)
{
	check_address(file); // 주소가 유효한 주소인지 체크
	struct file *file_obj = filesys_open(file); // 열고자 하는 파일 객체 정보를 받기

	// 제대로 파일이 생성되었는지 체크
	if (file_obj == NULL) {
		return -1;
	}
	int fd = add_file_to_fd_table(file); // 만들어진 파일을 쓰레드 내의 FDT애 추가

	if (fd == -1) {
		file_close(file_obj);
	}
	return fd;
}

/* 파일을 현재 프로세스의 FDT에 추가 */
int add_file_to_fd_table(struct file *file)
{
	struct thread *t = thread_current();
	struct file **fdt = t->file_descriptor_table;
	int fd = t->fdidx; // fd값은 2부터 출발
	
}