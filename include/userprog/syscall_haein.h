#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "userprog/process.h"
#include "threads/synch.h"


/* Project 2: system call */

#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1

void syscall_init (void);
void check_address(void *addr);

struct lock filesys_lock; /* 파일 시스템과 관련된 lock을 선언 */

#endif /* userprog/syscall.h */
