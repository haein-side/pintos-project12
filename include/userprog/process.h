#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* argument passing */
tid_t process_execute(const char *file_name); /* 프로그램을 실행 할 프로세스 생성 */
static void start_process(void *file_name); /* 프로그램을 메모리에 탑재하고 응용 프로그램 실행 */
void argument_stack(char **argv, int argc, struct intr_frame *if_); /* 함수 호출 규약에 따라 유저 스택에 프로그램 이름과 인자들을 저장 */
// void argument_stack(char **argv, int argc, void **rspp);
#endif /* userprog/process.h */
