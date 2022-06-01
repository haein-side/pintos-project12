#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"

#include "threads/flags.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/gdt.h"
#include "intrinsic.h"

#define MAX_FD_NUM	(1<<9) 

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void syscall_init (void);

void check_address(void *addr); 
struct file *fd_to_struct_filep (int fd);
int add_file_to_fd_table (struct file *file);

void halt (void);
void exit (int status);
bool create (const char *file, unsigned intial_size);
bool remove (const char *file);
int write (int fd, const void *buffer, unsigned size);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);

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

// struct lock filesys_lock;

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
		
	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {// f: 시스템콜을 호출한 유저 프로그램(caller)에 대한 정보를 저장해둔 구조체
	/* 유저 스택에 저장되어 있는 시스템 콜 넘버를 가져오기 */
	uintptr_t *rsp = f->rsp;
	check_address((void *) rsp);
	int sys_number = f-> R.rax; // rax: 레지스터에 시스템 콜 넘버가 저장되어 있음

	/* 인자 들어오는 순서 
		1번째 인자: %rdi
		2번째 인자: %rsi
		3번째 인자: %rdx
		번째 인자: %r10
		5번째 인자: %r8
		6번째 인자: %r9 	
	*/
	// printf("시스템콜 넘버 %d\n", sys_number);
	// TODO: Your implementation goes here.
	switch(sys_number) { // 인터럽트 프레임에 저장되어 있던 시스템 콜 넘버: 어떤 시스템 콜 함수를 호출하는지 저장해둔 넘버
		case SYS_HALT: 		// 0
			halt();
		case SYS_EXIT: 		// 1
			exit(f->R.rdi);
			break;
		// case SYS_FORK:  	// 2
		// 	fork(f->R.rdi);
		// 	// ?
		// case SYS_WAIT:
		// 	wait(f->R.rdi);
		// 	break;
		// case SYS_EXEC:
		// 	wait(f->R.rdi);
		case SYS_CREATE:
			create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			remove(f->R.rdi);
			break;
		case SYS_OPEN:
			open(f->R.rdi);
			break;
		case SYS_FILESIZE:
			filesize(f->R.rdi);
			break;
		case SYS_READ:
			read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		// case SYS_SEEK:
		// 	seek(f->R.rdi, f->R.rdx);
		// 	break;
		// case SYS_TELL:
		// 	tell(f->R.rdi);
			// break;
		// case SYS_CLOSE:
		// 	close(f->R.rdi);
		default:		   // default: case문들 중 어느 것도 해당되지 않을 때 실행됨
			thread_exit(); // 시스템콜 함수 진행 중인 커널 스레드를 종료시킴

	}

	// printf ("system call!\n");
	// printf("exit 위!\n");
	// thread_exit ();
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

	if (!is_user_vaddr(addr) || addr == NULL || pml4_get_page(curr->pml4, addr) == NULL) { 
		// 인자로 받은 주소값이 KERN_BASE보다 높은 값의 주소값을 가진 경우 or 매핑되지 않은 주소인 경우
		// 현재 프로세스를 종료시킴
		exit(-1);
		
	}

}

/* pintos 전체를 종료시키는 시스템 콜 */
void halt (void) {
	power_off(); /* Poweroff command for qemu(핀토스 테스트 돌려주는 에뮬레이터) */
}

/* 현재 프로세스를 종료시키는 시스템 콜 */
void exit (int status) {
	struct thread *t = thread_current();
	t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status); // Process Termination Mesage
	/* 정상적으로 종료됐다면 status는 0 */
	/* status: 프로그램이 정상적으로 종료됐는지 확인 */
	thread_exit(); // 핀토스는 프로세스가 곧 스레드 (1:1 매핑), USERPROG로 들어감 (요청자)
}

/* 파일을 새로 생성하는 시스템 콜 */
bool create (const char *file, unsigned intial_size) { // file: 생성할 파일의 이름 및 경로 정보, initial_size: 생성할 파일의 크기
	/* 성공이면 true, 실패면 false */
	check_address(file); // 파일의 VA가 VM 영역을 벗어났을 경우 현재 프로세스 종료해줌

	if (filesys_create(file, intial_size)) {
		return true;
	} else {
		return false;
	}
}

/* 파일을 제거하는 시스템 콜 */
bool remove (const char *file) {
	check_address(file);

	if (filesys_remove(file)) { // 이때, 파일을 제거하더라도 그 이전에 파일을 오픈했다면 
								// 해당 오픈 파일은 close되지 않고 그대로 켜진 상태로 남아있는다.
		return true;
	} else {
		return false;
	}
}

/* 열린 파일의 데이터를 기록하는 시스템 콜 */
int write (int fd, const void *buffer, unsigned size) {
	check_address(buffer);
	struct file *fileobj = fd_to_struct_filep(fd);
	int read_count;

	if (fd == STDOUT_FILENO) { // 파일 디스크립터 번호가 1인(출력을 하라는) 경우에 한해 값을 출력하는 함수를 작성
		putbuf(buffer, size); // 버퍼에 들어있는 값을 size만큼 출력
		read_count = size;
	}	
	
	else if (fd == STDIN_FILENO) {
		return -1;
	}

	else {
		
		lock_acquire(&filesys_lock);
		read_count = file_write(fileobj, buffer, size);
		lock_release(&filesys_lock);

	}

}

/* 파일에 접근해서 여는 함수 */
int open (const char *file) { // 해당 파일을 가리키는 포인터를 인자로 받음
	check_address(file); // 먼저 주소가 유효한지 체크
	struct file *file_obj = filesys_open(file); // 열려고 하는 파일 객체 정보 file_open (inode)를 filesys_open()으로 받기

	// 제대로 파일 "생성"됐는지 체크
	// 왜 open인데 생성??
	// file을 open한다는 건 새로운 파일 구조체 file을 만들고 거기에 접근하고 싶은 파일의 정보인 inode를 대입한 것을 반환해주는 것
	// file_open (inode)가 filesys_open(file)의 반환값으로 file_obj에 담김
	if (file_obj == NULL) {
		return -1; 
	}

	int fd = add_file_to_fd_table(file_obj); // 만들어진 파일을 스레드 내 fdt 테이블에 추가 -> 스레드가 해당 파일 관리 가능케

	// 만약 파일을 열 수 없으면 -1을 받음
	if (fd == -1) {
		file_close(file_obj);
	}
	
	return fd;
}

 /* 파일을 현재 프로세스의 fdt에 추가 */
int add_file_to_fd_table (struct file *file) { // file 구조체에 inode 관련 정보를 멤버로 넣어둔 걸 인자로 받음
	struct thread *t = thread_current();
	struct file **fdt = t->file_descriptor_table;
	int fd = t->fdidx; // fd 값은 2부터 출발 (0은 stdin - 표준입력, 1은 stdout -표준출력)
						// fd 는 파일 디스크립터로, FDT에서 해당 파일의 인덱스 번호
	while (t->file_descriptor_table[fd] != NULL && fd < FDCOUNT_LIMIT) {
		fd++;
	}

	if (fd >= FDCOUNT_LIMIT) {
		return -1;
	}
	t->fdidx = fd;
	fdt[fd] = file;
	return fd;
}

/* fd 값을 넣으면 해당 file을 반환하는 함수*/
// 예외 케이스 고려하여 fd 값이 0보다 작거나
// 파일 디스크립터 배열에 할당된 범위를 넘어서면 파일이 없다는 뜻이니 NULL을 반환 
// filesize()를 위한 함수
struct file *fd_to_struct_filep (int fd) {
	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return NULL;
	}

	struct thread *t = thread_current();
	struct file **fdt = t->file_descriptor_table;

	struct file *file = fdt[fd];
	return file;
}

/* 열려 있는 파일을 FDT에서 찾아 해당 파일의 크기를 반환하는 함수 */
int filesize (int fd) {
	struct file *fileobj = fd_to_struct_filep(fd); /* fd 값을 넣으면 해당 file 객체를 반환하는 함수 */

	/* 추가 */	
	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return;
	}

	if (fileobj == NULL) {
		return -1;
	}
	file_length(fileobj);
}

/* 열린 파일의 데이터를 읽는 시스템 콜 */
// 성공 시 읽은 바이트 수를 반환, 실패 시 -1 반환
// buffer: 읽은 데이터를 저장할 버퍼 주소 값, size: 읽을 데이터 크기
// fd 값이 0일 때 키보드의 데이터를 읽어 버퍼에 저장 (input_getc() 이용)
int read (int fd, void *buffer, unsigned size) {
	// 유효한 주소인지부터 체크
	check_address(buffer); // 버퍼 시작 주소 체크
	check_address(buffer + size - 1); // 버퍼 끝 주소도 유저 영역 내에 있는지 체크
	unsigned char *buf = buffer;
	int read_count;

	struct file *fileobj = fd_to_struct_filep(fd); /* fd 값을 넣으면 해당 file 객체를 반환하는 함수 */

	/* 추가 */
	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return;
	}

	if (fileobj == NULL) { // 파일 읽어오는 것 실패 시 -1 반환
		return -1;
	}

	/* STDIN일 때 */
	if (fd == STDIN_FILENO) { // 표준 입력: fd 값이 0일 때
		char key;
		for (int read_count = 0; read_count < size; read_count++) {
			key = input_getc();
			*buf++ = key;
			if (key == '\0') { // 엔터값
				break;
			}
		}
	}

	/* STDOUT일 때: -1 반환 */ 
	else if (fd == STDOUT_FILENO) { // 표준 출력: fd 값이 1일 때
		return -1;
	} 
	
	else {
		lock_acquire(&filesys_lock);
		read_count = file_read(fileobj, buffer, size); // 파일 읽어들일 동안에만 lock을 걸어줌
		lock_release(&filesys_lock);
	}

	return read_count;

}



