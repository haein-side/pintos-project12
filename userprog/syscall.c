#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"

#include "threads/synch.h"
#include "threads/init.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "kernel/console.h"
#include "filesys/filesys.h"
#include "filesys/file.h"


#define STDIN_FILENO 0
#define STDOUT_FILENO 1

#define MAX_FD_NUM (1<<9)
struct lock filesys_lock;

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(void *addr);
int add_file_to_fd_table(struct file *file);
void remove_file_to_fd_table(int fd);
struct file *fd_to_struct_filep (int fd);

void halt(void); 
void exit (int status);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int write (int fd, const void *buf, unsigned size);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);


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

	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) // 시스템 콜을 요청한 유저 프로그램의 정보가 담긴 구조체가 매개변수로 들어옴.
{
	/* 유저 스택에 저장되어 있는 시스템 콜 넘버를 가져오기 */
	uintptr_t *rsp = f->rsp;
	check_address((void *)rsp);	
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
	switch (sys_number) { // 들어온 syscall number에 맞는 동작을 수행
		case SYS_HALT : 
			halt();
		case SYS_EXIT : 
			exit(f->R.rdi);
			break;
		// case SYS_FORK : fork(f->R.rdi, f->R.rsi);
		// case SYS_EXEC : exec(f->R.rdi);
		case SYS_CREATE : 
			create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE : 
			remove(f->R.rdi);
			break;
		case SYS_OPEN : 
			open(f->R.rdi);
			break;
		case SYS_FILESIZE : 
			filesize(f->R.rdi);
			break;
		case SYS_READ : 
			read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE : 
			write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK : 
			seek(f->R.rdi, f->R.rdx);
			break;
		case SYS_TELL : 
			tell(f->R.rdi);
			break;
		case SYS_CLOSE : 
			close(f->R.rdi);
		default:
			thread_exit();
	}
	// printf ("system call!\n");
	// thread_exit ();
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
	printf("%s: exit(%d)\n", t->name, status); // Process Termination Message
	/* 정상적으로 종료되었다면 status = 0 */
	/* status : 프로그램이 정상적으로 종료됐는지 확인 */
	thread_exit();
}
/* 파일을 생성하는 시스템콜 */
bool create (const char *file, unsigned initial_size)
{	
	/* 성공이면 true, 실패면 false */
	check_address(file);
	bool result = filesys_create(file, initial_size);
	return result;
}

/* 파일을 제거하는 시스템콜 */
bool remove (const char *file)
{
	/* 파일을 제거하더라도, 그 이전에 파일을 오픈했다면 해당 오픈 파일은 close되지 않고,
	 * 켜진 상태로 남아있는다. */
	check_address(file);
	bool result = filesys_remove(file);
	return result;
}
/* 버퍼에 있는 내용을 fd파일에 작성. 파일에 작성한 바이트 반환 */
int write (int fd, const void *buf, unsigned size) 
{
    check_address(buf);
    struct file *file_obj = fd_to_struct_filep(fd);
    int read_count;

    if (file_obj == NULL){
        return -1;
    }

    lock_acquire(&filesys_lock);
    if (fd == STDOUT_FILENO) { // STDOUT일 경우, 화면에 출력을 해주어야 함.
        putbuf(buf, size); // 버퍼에서 size만큼 읽어와 출력
        read_count = size;
    }

    else if (fd == STDIN_FILENO) { // STDIN일 경우, 해당 함수와 관련이 없으므로 -1을 반환 
        lock_release(&filesys_lock);
        return -1;
    }

    else if (fd >= 2) { // 나머지의 경우 버퍼로부터 값을 읽어와 해당 파일에 작성해준다.
        if (file_obj == NULL) {
			lock_release(&filesys_lock);
            exit(-1);
        }   
        read_count = file_write(file_obj, buf, size);
    }
    lock_release(&filesys_lock);
    
    return read_count;
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
	int fd = add_file_to_fd_table(file); // 만들어진 파일을 쓰레드 내의 FDT에 추가

	// 만약 파일을 열 수 없으면 -1. 따라서 종료시켜줌
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
	int fd = t->fdidx; // fd값은 2부터 시작
	
	while (t->file_descriptor_table[fd] != NULL && fd < FDCOUNT_LIMIT) {
		fd++;
	}
	
	if (fd >= FDCOUNT_LIMIT)
		return -1;

	t->fdidx = fd;
	fdt[fd] = file;
	return fd;
}

/* 파일 테이블에서 fd 제거 */
void remove_file_to_fd_table(int fd)
{
	struct thread *t = thread_current();

	// Error - invalid fd
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;

	t->file_descriptor_table[fd] = NULL;
}

/* fd 값을 넣으면 해당 file을 반환하는 함수 */
struct file *fd_to_struct_filep (int fd) 
{
	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return NULL;
	}
	struct thread *t = thread_current();
	struct file **fdt = t->file_descriptor_table;

	struct file *file = fdt[fd];
	return file;
}

/* file size를 반환하는 함수 */ 
int filesize (int fd)
{
	struct file *file_obj = fd_to_struct_filep(fd);
	
	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return;
	}

	if (file_obj == NULL){
		return -1;
	}
	file_length(file_obj);
}

/* 해당 파일로부터 값을 읽어 버퍼에 넣는 함수 */
int read (int fd, void *buffer, unsigned size)
{
	// 유효한 주소인지 체크
	check_address(buffer); // 버퍼 시작 주소 체크 
	check_address(buffer + size - 1); // 버퍼 끝 주소도 유저 영역에 있는지 체크
	unsigned char *buf = buffer;
	int read_count;

	struct file *file_obj = fd_to_struct_filep(fd);

	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return;
	}

	if (file_obj == NULL) {
		return -1;
	}

	/* STDIN일 때 */
	/*	fd로부터 size 바이트 크기만큼 읽어서 버퍼에 넣어준다.
	*/
	if (fd == STDIN_FILENO) {
		char key;
		for (read_count=0; read_count < size; read_count++) {
			key = input_getc(); // 키보드 입력을 읽어오는 역할의 함수
			*buf++ = key;
			if (key == '\0') { // 엔터값 
				break;
			}
		}
	}
	/* STDOUT일 때 -1 반환 */
	/* 파일을 읽어들일 수 없는 케이스, 대표적으로 STDOUT일 때.*/
	else if (fd == STDOUT_FILENO) {
		return -1;
	}
	/* 표준 입출력이 아닌 경우에는 */
	/* fd로부터 파일 객체를 찾은 뒤 size바이트 크기만큼 파일을 읽어 버퍼에 넣어준다. */
	else {
		lock_acquire(&filesys_lock);
		read_count = file_read(file_obj, buffer, size); // 파일을 읽어들이는 동안은 lock을 걸어줌
		lock_release(&filesys_lock);
	}
	return read_count;
}
/* fd를 이용해 파일을 찾은 뒤, 해당 파일 객체의 pos를 입력받은 position으로 변경하는 함수 */
void seek (int fd, unsigned position)
{
	struct file *file_obj = fd_to_struct_filep(fd);
	check_address(file_obj);
	if (file_obj == NULL || file_obj <= 2) {
		return;
	}
	file_seek(file_obj, position);
}

/* 파일의 시작점부터 현재 위치까지의 offset을 반환 */
unsigned tell (int fd)
{
	if (fd < 0 || fd < 2 || fd >= FDCOUNT_LIMIT) {
		return;
	}
	/* 파일을 읽으려면 어디서부터 읽어야하는지에 대한 위치 pos를 
	 * 파일 내 구조체 멤버에 정보로 저장한다.
	 * fd값을 인자로 넣어주면 해당 파일의 pos를 반환하는 함수. */
	struct file *file_obj = fd_to_struct_filep(fd);
	check_address(file_obj);
	if (file_obj == NULL || file_obj <= 2) {
		return;
	}
	return file_tell(fd);
}

void close (int fd) {
	if (fd < 2) {
		return;
	}
	struct thread *curr = thread_current();
	struct file *file_obj = fd_to_struct_filep(fd);
	check_address(file_obj);
	if (file_obj == NULL) {
		return;
	
	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return;
	}
	curr->file_descriptor_table = NULL;
	}
}