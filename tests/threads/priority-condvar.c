/* Tests that cond_signal() wakes up the highest-priority thread
   waiting in cond_wait(). */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static thread_func priority_condvar_thread;
static struct lock lock;
static struct condition condition;

void
test_priority_condvar (void) 
{
  int i;
  
  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  lock_init (&lock); // lock이 풀린 1인 상태로 초기화 sema_init (&lock->semaphore, 1);
  cond_init (&condition); // condition variable 자료구조를 초기화 list_init (&cond->waiters);
                          // condition 마다 waiters 리스트를 초기화해주는 것 (여기선 특수하게 condition이 전역변수로 1개)
  
  thread_set_priority (PRI_MIN);
  for (i = 0; i < 10; i++) 
    {
      int priority = PRI_DEFAULT - (i + 7) % 10 - 1;
      char name[16];
      snprintf (name, sizeof name, "priority %d", priority);
      thread_create (name, priority, priority_condvar_thread, NULL); // idle이 thread_create해줌
    }

  for (i = 0; i < 10; i++) 
    {
      lock_acquire (&lock);
      msg ("Signaling...");
      cond_signal (&condition, &lock);
      lock_release (&lock);
    }
}

static void
priority_condvar_thread (void *aux UNUSED) 
{
  msg ("Thread %s starting.", thread_name ());
  lock_acquire (&lock); // cond_wait에서 리스트에 하나씩 넣어주려고 lock을 걸어줌
  cond_wait (&condition, &lock);
  msg ("Thread %s woke up.", thread_name ()); // 다시 실행됨
  lock_release (&lock);
}
