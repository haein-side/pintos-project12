#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#define FDT_PAGES 3
#define FDCOUNT_LIMIT FDT_PAGES * (1<<9) // 3 * 512 (limit fdidx)
										 // 페이지의 크기는 4KB(1<<12)인데, 파일 구조체 주소 크기가 8byte(1<<3)이므로, 
										 // 이를 분리하면 512byte (1<<9)만큼의 공간을 할당받는 것과 같다.
										 // 즉, 파일 구조체를 저장하기 위해 4KB만큼의 페이지 공간을 할당해주는 것이다.
#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* 쓰레드 ID (Thread identifier.) */
	enum thread_status status;          /* 쓰레드 상태 (Thread state.) */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
	int64_t wakeup_tick; 				/* 깨어나야 할 tick */

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */

	/* priority donation */
	int init_priority; 					/* 우선순위를 donation 받을 때, 자신의 원래 우선 순위를 저장할 수 있는 필드 */
	struct lock *wait_on_lock;			/* 해당 쓰레드가 대기하고 있는 lock 자료 구조의 주소를 저장하는 필드 */
	
	/* multiple priority를 구조체 선언 */
	struct list donations; 				/* 자신에게 priority를 donate한 쓰레드의 리스트 */
	struct list_elem donation_elem;  	/* priority를 donate한 쓰레드들의 리스트를 관리하기 위한 element 
										이 element를 통해 자신이 우선 순위를 donate한 쓰레드의 donates 리스트에 연결*/
	
#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */

	/* --- Project2: User programs - system call --- */
	struct file **file_descriptor_table; // FDF
	int fdidx; // fd idx

	struct list child_list;			// _fork(), wait() 구현 때 사용
	struct list_elem child_elem; 	// _fork(), wait() 구현 때 사용
	
	int exit_status; // exit(), wait() 구현 때 사용


	struct intr_frame parent_if;	// _fork() 구현 때 사용, __do_fork() 함수
	struct semaphore fork_sema;
	struct semaphore free_sema;
	struct semaphore wait_sema;

	struct file *running;

	int stdin_count;
	int stdout_count;
};
/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

/* project1 : prority scheduling */
void test_max_priority(void);
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);


/* project1 : priority donation */
void donate_priority(void);
void remove_with_lock(struct lock *lock); 
void refresh_priority(void);
#endif /* threads/thread.h */
