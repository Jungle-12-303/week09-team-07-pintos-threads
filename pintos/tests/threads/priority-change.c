/* Verifies that lowering a thread's priority so that it is no
   longer the highest-priority thread in the system causes it to
   yield immediately. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/thread.h"

static thread_func changing_thread;

// 중요도 변경하는 테스트 함수
void
test_priority_change (void) 
{
  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  //thread2 라는 default보다 1 높은 쓰레드 추가
  msg ("Creating a high-priority thread 2.");
  thread_create ("thread 2", PRI_DEFAULT + 1, changing_thread, NULL);
  msg ("Thread 2 should have just lowered its priority.");
  
  // 현재 쓰레드의 priority를 -2 함
  thread_set_priority (PRI_DEFAULT - 2);
  msg ("Thread 2 should have just exited.");
}

// 실제 변경요청 하는 테스트 함수 
static void
changing_thread (void *aux UNUSED) 
{
  msg ("Thread 2 now lowering priority.");

  thread_set_priority (PRI_DEFAULT - 1);
  msg ("Thread 2 exiting.");
}