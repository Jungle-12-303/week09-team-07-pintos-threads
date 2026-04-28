#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <list.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
// 틱 회수
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
// 틱당 반복 회수. timer_calibrate()로 초기화 된다.
static unsigned loops_per_tick;

// 실행 중인 작업을 일시 중단하고, 해당 상황을 처리한 후 기존 작업으로 돌아가는 과정
// interrupt 의미
static intr_handler_func timer_interrupt;
// unsigned 만 작성하면 int형이 축약된 것.
static bool too_many_loops (unsigned loops);
// loops 만큼 지연 제공
static void busy_wait (int64_t loops);
// (num / denom) 만큼 쉼 
static void real_time_sleep (int64_t num, int32_t denom);

// 직접 구현해야 할 thread_sleep
static void thread_sleep (int64_t timer_ticks);

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
/* 8254 프로그래밍 가능 인터럽트 타이머(PIT)를 초당 PIT_FREQ번 인터럽트되도록 설정하고,
해당 인터럽트를 등록합니다. */
void
timer_init (void) {
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
/* 짧은 지연을 구현하는 데 사용되는 loops_per_tick 값을 보정합니다. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */

	/* 틱당 반복 횟수를 최대 2의 거듭제곱으로 근사합니다.
		여전히 타이머 틱 하나보다 작습니다. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	/* loops_per_tick의 다음 8비트를 다듬습니다. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
/* 운영체제가 부팅된 이후 경과된 타이머 틱 수를 반환합니다. */
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */

/* THEN 이후 경과된 타이머 틱 수를 반환합니다.
이 값은 timer_ticks() 함수가 반환했던 값이어야 합니다. */
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

/* Suspends execution for approximately TICKS timer ticks. */
/* 약 TICKS 타이머 틱 동안 실행을 일시 중단합니다. */
void
timer_sleep (int64_t ticks) {
	if (ticks <= 0) {
		return;
	}
	// start에 현재 tick 값 보관
	int64_t start = timer_ticks();
	
	// 인터럽트 잠시 종료.
	// 인터럽트는 단순 틱 증가시킬 때 제외하고도 항상 부를 수 있기 때문에
	// 미리 종료해주고 작업하는게 안전한 방식
	enum intr_level old_level = intr_disable();

	// 쓰레드를 timer.c에서 불러올 필요는 없을 것 같음.
	thread_sleep(ticks + start);

	// 인터럽트를 기존 상태로 되돌림.
	intr_set_level(old_level);
}


/* Suspends execution for approximately MS milliseconds. */
/* 약 MS 밀리초 동안 실행을 일시 중단합니다. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
/* 약 마이크로초 동안 실행을 일시 중단합니다. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
/* 약 NS 나노초 동안 실행을 일시 중단합니다. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
/* 타이머 통계를 출력합니다. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
// 인터럽트 핸들러
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;

	thread_tick ();

	// TODO
	thread_wakeup(ticks);	
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) {
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait (loops);

	/* If the tick count changed, we iterated too long. */
	barrier ();
	return start != ticks;
}

/* 간단한 루프를 LOOPS번 반복하여 짧은 지연을 구현합니다.


코드 정렬이 실행 시간에 상당한 영향을 미칠 수 있으므로 NO_INLINE으로 표시했습니다.
따라서 이 함수가 다른 위치에서 다르게 인라인될 경우
결과가 예측하기 어려울 수 있습니다. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
// num / denom 만큼 sleep 시도
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.
	// 타이머 틱으로 변환 및 나머지 버림

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	// ticks = num * 100 / denom
	// (숫자 / 분모)초
	// ---------------------- = 숫자 * 타이머 주파수 / 분모 틱

	// 1초 / 타이머 주파수 틱

	// 틱 = 숫자 * 100 / 분모
	// 분모 1이면 숫자 * 100이니까 1초당 100번임.

	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		// 최소 한번의 틱이 완료 될 때 까지 대기합니다.
		// 다른 프로세스에게 CPU를 양보하기 위해 timer_sleep을 사용합니다.

		// 이거 매개변수 값 수정해야할거 같은데
		// ticks가 OS ticks보다 낮은건 당연하잖아
		timer_sleep (ticks);
	} else {
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		// 1 tick보다 짧은 시간을 정확하게 대기할 때는 busy-wait 루프를 씁니다. 
		// 오버플로우 방지를 위해 분자/분모를 1000으로 나눠서 계산합니다.
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}