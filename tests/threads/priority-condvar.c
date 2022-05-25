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
static struct condition condition; // waiters가 담겨있는 condition 구조체

void
test_priority_condvar (void) 
{
  int i;
  
  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  lock_init (&lock); // lock 초기화
  cond_init (&condition); // condition 구조체 초기화

  thread_set_priority (PRI_MIN); // 현재 실행 중인 쓰레드의 우선 순위를 0으로 지정
  for (i = 0; i < 10; i++) 
    {
      int priority = PRI_DEFAULT - (i + 7) % 10 - 1; // 우선순위 책정
      char name[16];
      snprintf (name, sizeof name, "priority %d", priority);
      thread_create (name, priority, priority_condvar_thread, NULL);
      // 쓰레드를 생성하면서
    }

  for (i = 0; i < 10; i++) 
    {
      lock_acquire (&lock); // lock 획득
      msg ("Signaling..."); 
      cond_signal (&condition, &lock); // 잠들어 있던 쓰레드 깨우기
      lock_release (&lock); // lock 반납
    }
}

static void priority_condvar_thread (void *aux UNUSED) 
{
  msg ("Thread %s starting.", thread_name ());
  lock_acquire (&lock); // lock 획득
  cond_wait (&condition, &lock);
  msg ("Thread %s woke up.", thread_name ());
  lock_release (&lock); // lock 반환
}
