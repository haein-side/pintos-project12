#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* -------------------- pjt1 ------------------------- */

// status가 THREAD_BLOCKED인 스레드를 관리하기 위한 리스트
static struct list sleep_list;
// sleep_list에서 대기중인 스레드들 중 가장 빨리 일어나야 하는 스레드의 wakeup_tick(즉, 최솟값) 저장
static int64_t next_tick_to_awake;

/* -------------------- pjt1 ------------------------- */

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */

/* init code */
void
thread_init (void) {
	/* 현재 interrupt가 disabled(OFF)인지 enabled(ON)인지 확인 후 disable일 경우 계속 진행 */
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context 
	 * synchronization을 위한 lock과 scheduling을 위한 list의 초기화를 담당
	 */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&destruction_req);

	/* -------------------- pjt1 ------------------------- */
	// sleep(BLOCKED)상태인 스레드들을 연결해놓은 리스트를 초기화
	list_init (&sleep_list);

	// 최솟값을 찾아가야 하기 때문에 정수 최댓값으로 초기화해줌
	next_tick_to_awake = INT64_MAX;
	/* -------------------- pjt1 ------------------------- */

	/* Set up a thread structure for the running thread. 
	main() 함수에서 호출되는 스레드 관련 초기화 함수?
	*/
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();

}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	// 스레드를 초기화 할 때는 THREAD_BLOCKED 상태로 초기화 함. 아래서 unblock을 통해 ready_list에 들어갈 수 있도록 해줌
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	// kernel이 알고 있는 thread가 kernel_thread
	// CPU가 각각의 thread까지 스케줄링(or context switch)해준다.
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function; // 인자로 받아온 function이 실행됨(?)
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;

	/* Add to run queue(=ready_list) */
	thread_unblock (t);

	/* 새로 생성된 thread의 우선순위가 높은 경우 RUNNING하고 있는 스레드가 CPU 점유권을 양보하도록 yield 호출 */
	if (priority > thread_get_priority()) {
		thread_yield();
	}

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	// 우선 순위로 정렬되어 ready_list에 들어가도록 수정
	list_insert_ordered (&ready_list, &t->elem, cmp_priority, NULL);
	// list_push_back (&ready_list, &t->elem);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* Returns the name of the running thread. */
// thread_create 할 때 name을 넣어줘야하는데, 이 때 name을 const char 형식으로 넣어줘야 함
// name은 지정해주기 나름인데, 다른 파일에서 name에 priority를 저장해두면(ex. snprintf 함수 등) include된 헤더를 통해 thread_name을 사용할 수 있음
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
/* 
thread_yield() 함수 설명 
thread_current() 
	현재 실행 되고 있는 thread를 반환 
intr_disable() 
	인터럽트를 비활성하고 이전 인터럽트의 상태를 반환 
intr_set_level(old_level) 
	인자로 전달된 인터럽트 상태로 인터럽트를 설정 하고 이전 인터럽트 상태를 반환 
list_push_back(&ready_list, &cur->elem) 
	주어진 entry를 list의 마지막에 삽입 
schedule()  
	컨텍스트 스위치 작업을 수행
*/

void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ()); // 

	old_level = intr_disable ();
	if (curr != idle_thread)
		// 우선순위로 정렬되어 삽입되도록 수정
		list_insert_ordered (&ready_list, &curr->elem, cmp_priority, NULL);
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/*
next_tick_to_awake가 깨워야 할 스레드 중 가장 작은 tick을 갖도록 업데이트
sleep_list에 스레드가 새로 들어올 때마다 wakeup_tick을 비교하며 갱신해줘야 함
why? 가장 나중에 들어온 스레드가 가장 먼저 깨워야 할 스레드일수도 있기 때문
*/
void
update_next_tick_to_awake(int64_t ticks) {
	next_tick_to_awake = (next_tick_to_awake>ticks) ? ticks : next_tick_to_awake;
}

/*
next_tick_to_awake 값을 반환하는 함수
*/
int64_t
get_next_tick_to_awake(void) {
	return next_tick_to_awake;
}


/*
스레드를 재우는 thread_sleep 구현
sleep 해줄 스레드를 sleep list에 추가하고 status를 THREAD_BLOCKED으로 만들어준다.
이 때 idle thread를 sleep시켜준다면 CPU가 실행 상태를 유지할 수 없어 종료되므로 예외처리를 해 주어야 한다.
sleep 수행 중에는 인터럽트를 받아들이지 않는다
*/
void
thread_sleep(int64_t ticks) {
	/* 현재 실행되고 있는 스레드 가져오기 */
	struct thread* cur = thread_current();

	/* interrupt disable */
	enum intr_level old_level;	// interrupt 상태를 담는 변수 정의 ON or OFF
	ASSERT(!intr_context())	  	// external interrupt 발생하면 ASSERT
	old_level = intr_disable(); // disable 시키고 ON(이전 상태) 반환

	ASSERT(cur != idle_thread); 	// 현재 thread가 idle thread라면 종료

	cur->wakeup_tick = ticks;	// wakeup_tick 업데이트
	update_next_tick_to_awake(cur->wakeup_tick); // next_tick_to_awake 업데이트
	list_push_back (&sleep_list, &cur->elem);	 // sleep_list에 추가

	/* 스레드를 sleep 시킴*/
	thread_block(); // 현재 스레드의 status를 BLOCKED로 바꿔주고, schedule 함수가 실행되어 다음 스레드의 status를 RUNNING으로 바꿔줌(schedule에서 현재 스레드의 상태가 RUNNING이 아닌지도 확인함. 그래서 BLOCKED로 stauts를 바꿔줘야 함)

	/* interrupt enable */
	intr_set_level(old_level); // old_level, 즉 ON을 넣어주어 eneable로 만들어줌

}

/*
스레드를 깨우는 thread_awake 구현
sleep list에서 자고 있는 스레드를 깨워 sleep list에서 제거한 후 ready list에 넣어줌
이 때 status도 꼭 바꿔줘야 함
*/
void
thread_awake(int64_t ticks) {
	struct list_elem* cur = list_begin(&sleep_list);
	struct thread* t;

	// 최솟값을 찾아가야 하기 때문에 정수 최댓값으로 초기화해줌
	next_tick_to_awake = INT64_MAX;

	/* sleep list 순회 */
	while(cur != list_end(&sleep_list)) {
		t = list_entry(cur, struct thread, elem); // sleep list에서 현재 스레드(elem)를 가져옴

		if (ticks >= t->wakeup_tick) { // 깨울 시간이 지났으면
			cur = list_remove(&t->elem); // sleep list에서 제거하고
			thread_unblock(t);			// 스레드 t를 ready list에 넣어주고 status도 READY로 바꿔줌
		}
		else { // 아직 깨울 시간 안됐으면 다음 스레드로 넘어가자
			cur = list_next(cur);
			update_next_tick_to_awake(t->wakeup_tick); // next_tick_to_awake 현행화
		}
	}
}

/*
첫 번째 인자의 우선순위가 높으면 1을 반환, 두 번째 인자의 우선순위가 높으면 0을 반환
stdbool을 include하면 True = 1, False = 0으로 알아서 대체됨
따라서 return thread_b->priority < thread_a->priority 으로 써줘도 됨!
*/
bool
cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
	struct thread* thread_a;
	struct thread* thread_b;

	thread_a = list_entry(a, struct thread, elem);
	thread_b = list_entry(b, struct thread, elem);
	if (thread_b->priority < thread_a->priority) {
		return 1;
	}
	else {
		return 0;
	}
}

/*
현재 스레드의 우선 순위와 ready_list에서 가장 높은 우선 순위를 비교하여 현재 스레드의 우선 순위가 더 작다면 thread_yield()
priority preemption을 해준다고 생각하면 됨
*/
void
test_max_priority(void) {
	// 예외처리
	if(list_empty(&ready_list)) {
		return;
	}

	struct list_elem* max = list_begin(&ready_list);
	struct thread* t;
	
	t = list_entry(max, struct thread, elem);
	
	if (thread_get_priority () < t->priority) {
		thread_yield();
	}
}

/* 
Sets the current thread's priority to NEW_PRIORITY. 
현재 스레드의 우선 순위를 새 우선 순위로 설정합니다. 현재 스레드가 더 이상 가장 높은 우선 순위를 갖지 않으면 양보합니다.
test_max_priority 함수 호출 필요
*/
// void
// thread_set_priority (int new_priority) {
// 	thread_current ()->priority = new_priority;
// 	// 스레드 우선순위 변경시 donation의 발생을 확인하고 우선순위 변경을 위해 donation_priority 함수 추가
// 	// donation_priority();
// 	test_max_priority ();
// }

void
thread_set_priority (int new_priority) {
	thread_current ()->initial_priority = new_priority;
	refresh_priority ();	// 현 thread의 donations 리스트에 따라 우선순위 갱신
	test_max_priority ();	// 우선순위를 갱신한 경우에도 확인 필요
}

/* 
Returns the current thread's priority.
현재 스레드의 우선 순위를 반환합니다. 우선 기부가 있는 경우 더 높은(기부) 우선 순위를 반환합니다.
*/
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;

	/* -------------------- pjt2 ------------------------- */
	t->initial_priority = priority;
	list_init(&t->donations);
	t->wait_on_lock = NULL;
	/* -------------------- pjt2 ------------------------- */
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used bye the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

/*
'현재 thread가 기다리고 있는 lock'과 연결된 '모든 threads를 순회'하며 '현재 thread의 우선순위'를 'lock을 보유하고 있는 thread'에게 '기부'
여기서 '연결된'이라는 말의 의미는, 연쇄적인 lock 요청(C의 홀더는 B를 요청, B의 홀더는 A를 요청)으로 인해 원하는 lock을 얻지 못하는 경우를 고려하여 해당 lock을 획득하는 것과 관계 있는 모든 thread에게 우선순위를 기부해야 함을 의미한다.
(하나의 thread가 복수개의 lock을 획득해야하는 경우는 lock_acquire이 반복실행되며 이 함수도 lock의 횟수만큼 실행되어야 함)
즉 nested donation을 고려해야 하며 이 때 nested depth는 8로 제한한다.
*/
void
donate_priority (void) {
	int depth = 0; // nested depth
	struct thread *t = thread_current ();
	int cur_priority = t->priority;
	
	while (depth < 8) {
		depth++;
		if (!t->wait_on_lock) {		 // 순회중인 thread가 원하는 lock이 없다면 nested donation 중지
			break;
		}

		t = t->wait_on_lock->holder; // t가 원하는 lock을 가진 holder를 새로운 t로 계속 갱신(lock 획득을 위해 연결된 thread를 순회)
		t->priority = cur_priority;	// 위에서 갱신되는 t의 priority를 모두 cur_priority로 갱신
	}
}

/*
lock을 해지했을 때, donations 리스트에서 해당 엔트리를 삭제하기 위한 함수
현재 thread의 donations 리스트를 확인하여 해지할 lock을 보유하고 있는 엔트리를 삭제
*/
// void
// remove_with_lock (struct lock *lock) {
// 	struct thread *t = thread_current ();
// 	struct list_elem *e = list_begin(&t->donations);

// }


// 스레드의 우선순위가 변경 되었을 때, donation을 고려하여 우선순위를 다시 결정하는 함수
void
refresh_priority (void) {
	struct thread *curr = thread_current ();
	curr->priority = curr->initial_priority;

	if (!list_empty(&curr->donations)) {
		list_sort(&curr->donations, cmp_priority_d, NULL);			// 정렬
		struct thread *max = list_entry (list_front (&curr->donations), struct thread, donation_elem); // donations 내의 최댓값
		if (max->priority > curr->priority)							// max값이 제일 크면 우선순위 바꿔줌
			curr->priority = max->priority;
	}
}
// a와 b의 우선순위를 비교 (a > b ? 1 : 0)
bool
cmp_priority_d (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
	struct thread *t_a = list_entry (a, struct thread, donation_elem);
	struct thread *t_b = list_entry (b, struct thread, donation_elem);
	return (t_a->priority > t_b->priority);
}

void
remove_with_lock(struct lock *lock) {
	struct thread *curr = thread_current ();
	struct list_elem *e;
	for (e = list_begin (&curr->donations); e != list_end (&curr->donations); e = list_next (e)) {
		struct thread *t = list_entry (e, struct thread, donation_elem);
		if (t->wait_on_lock == lock)
			list_remove (&t->donation_elem);
	}
}