#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
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
#include "threads/palloc.h"
#include "include/lib/string.h"



#define STDIN_FILENO 0
#define STDOUT_FILENO 1

#define MAX_FD_NUM (1<<9)
struct lock filesys_lock;

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(void *addr);
int add_file_to_fd_table(struct file *file);
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
tid_t fork (const char *thread_name, struct intr_frame *f);
int exec (char *file_name);
int wait(tid_t pid);
void process_close_file(int fd);

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
			break;
		case SYS_EXIT : 
			exit(f->R.rdi);
			break;
		case SYS_FORK :
			f->R.rax = fork(f->R.rdi, f);
			break;
		case SYS_EXEC : 
			if (exec(f->R.rdi) == -1){
				exit(-1);
				}
			break;
		case SYS_WAIT :
			f->R.rax = wait(f->R.rdi);
			break;
		case SYS_CREATE : 
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE : 
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN : 
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE : 
			f->R.rax = filesize(f->R.rdi);
			break;
      	case SYS_READ:
         	f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK : 
			seek(f->R.rdi, f->R.rdx);
			break;
		case SYS_TELL : 
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE : 
			close(f->R.rdi);
			break;
		default:
			thread_exit();
			break;
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
	check_address(file);	// 포인터가 가리키는 주소가 유저영역의 주소인지 확인
	return filesys_remove(file); // 파일 이름에 해당하는 파일을 제거
}

/* 버퍼에 있는 내용을 fd파일에 작성. 파일에 작성한 바이트 반환 */
int write (int fd, const void *buffer, unsigned size) {
   check_address(buffer);
   struct file *fileobj = fd_to_struct_filep(fd);
   unsigned char *buf = buffer;
   int read_count;

   lock_acquire(&filesys_lock);

   if (fd == STDOUT_FILENO) { // 파일 디스크립터 번호가 1인(출력을 하라는) 경우에 한해 값을 출력하는 함수를 작성
      putbuf(buf, size); // 버퍼에 들어있는 값을 size만큼 출력
      read_count = size;
   }   
   
   else if (fd == STDIN_FILENO) {
      lock_release(&filesys_lock);
      return -1;
   }

   else if (fd >= 2) {
      if (fileobj == NULL){
         lock_release(&filesys_lock);
         exit(-1);
      }
      read_count = file_write(fileobj, buf, size);
   }
   lock_release(&filesys_lock);
   return read_count;
}



/* 사용자 프로세스가 파일에 접근하기 위해 요청하는 시스템 콜 */
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

/* 파일을 현재 프로세스의 FDT에 추가 */
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

/* fd 값을 넣으면 해당 file을 반환하는 함수 */
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
	return file_length(file_obj);
}

/* 해당 파일로부터 값을 읽어 버퍼에 넣는 함수 */
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

/* fd를 이용해 파일을 찾은 뒤, 해당 파일 객체의 pos를 입력받은 position으로 변경하는 함수 */
/* 파일의 시작점부터 현재 위치까지의 offset을 반환 */
void seek(int fd, unsigned position) {
	if (fd < 2) {
		return;
	}

	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return;
	}
	struct file *file = fd_to_struct_filep(fd);
	check_address(file);
	if (file == NULL) {
		return;
	}
	file_seek(file, position);
}

void close (int fd) {
	if(fd < 2) return;
	struct file *f = fd_to_struct_filep(fd);

	if(f == NULL)
		return;
	remove_file_to_fd_table(fd);
	file_close(f);
}

tid_t fork (const char *thread_name, struct intr_frame *f)
{
	return process_fork(thread_name, f);
}
/* 현재 프로세스를 명령어로 입력 받은 실행파일로 변경하는 역할 */
int exec (char *file_name)
{
	check_address(file_name);
	int size = strlen(file_name) + 1;
	char *fn_copy = palloc_get_page(PAL_ZERO); // fn_copy로 복사하는 이유는 caller 함수와 load사이의 race condition을 방지하기 위함.
	if (fn_copy == NULL) {
		exit(-1);
	}
	strlcpy(fn_copy, file_name, size);

	if (process_exec(fn_copy) == -1) 	// 해당 프로세스를 메모리에 load하고 정소를 스택에 쌓는다.
		return -1;	// 오류 발생할 경우 -1 리턴

	NOT_REACHED();
	return 0;
}

/* Wait for a child process to die. */
int wait(tid_t pid)
{
	process_wait(pid);
}

void process_close_file(int fd)
{
	if (fd < 0 || fd > FDCOUNT_LIMIT)
		return NULL;
	thread_current()->file_descriptor_table[fd] = NULL;
}

/* 파일 테이블에서 fd 제거 */
void remove_file_to_fd_table(int fd)
{
	struct thread *t = thread_current();

	// Error - invalid fd
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return NULL;

	t->file_descriptor_table[fd] = NULL;
}


unsigned tell (int fd) {
	if (fd <2) {
		return;
	}

	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return;
	}
	struct file *file = fd_to_struct_filep(fd);
	check_address(file);
	if (file == NULL) {
		return;
	}
	return file_tell(fd);
}