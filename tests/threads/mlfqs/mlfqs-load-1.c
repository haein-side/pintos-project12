/* Verifies that a single busy thread raises the load average to
   0.5 in 38 to 45 seconds.  The expected time is 42 seconds, as
   you can verify:
   perl -e '$i++,$a=(59*$a+1)/60while$a<=.5;print "$i\n"'

   Then, verifies that 10 seconds of inactivity drop the load
   average back below 0.5 again. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

void
test_mlfqs_load_1 (void) 
{
  int64_t start_time;
  int elapsed;
  int load_avg;
  
  ASSERT (thread_mlfqs);

  msg ("spinning for up to 45 seconds, please wait..."); // 최대 45초 동안 기다려야 할 수 있다

  start_time = timer_ticks ();
  for (;;) 
    {
      load_avg = thread_get_load_avg ();
      ASSERT (load_avg >= 0);
      elapsed = timer_elapsed (start_time) / TIMER_FREQ; // 시작하고 elapsed 초 경과

      /* load_avg가 50 초과 100 이하면 문제 없음 */
      if (load_avg > 100)
        fail ("load average is %d.%02d "
              "but should be between 0 and 1 (after %d seconds)",
              load_avg / 100, load_avg % 100, elapsed);
      else if (load_avg > 50)
        break;
      /* 45초 이상 걸릴 경우 */
      else if (elapsed > 45)
        /* load_avg가 0.5 미만 */
        fail ("load average stayed below 0.5 for more than 45 seconds");
    }

  /* 비정상: load_avg가 0.5에 도달하는 데 너무 짧은 시간(38초 미만)이 걸렸음 */
  if (elapsed < 38)
    fail ("load average took only %d seconds to rise above 0.5", elapsed);
  /* 정상: load_avg가 0.5에 도달하는 데 38초 이상 45초 미만이 걸림 */
  msg ("load average rose to 0.5 after %d seconds", elapsed);

  msg ("sleeping for another 10 seconds, please wait..."); // 10초 더 잠, 기다려..
  timer_sleep (TIMER_FREQ * 10); // 10초 재우기

  load_avg = thread_get_load_avg ();
  /* 비정상: load_avg가 음수가 됨 */
  if (load_avg < 0)
    fail ("load average fell below 0");
  /* 비정상: load_avg가 10초 넘도록 0.5 이상의 값을 유지함 */
  if (load_avg > 50)
    fail ("load average stayed above 0.5 for more than 10 seconds");
  /* 정상: 10초가 지난 후 load_avg가 다시 0.5 아래로 떨어졌음 */
  msg ("load average fell back below 0.5 (to %d.%02d)",
       load_avg / 100, load_avg % 100);

  pass ();
}
