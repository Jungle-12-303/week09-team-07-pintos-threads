# 9주차 PintOS Project 1: Threads KAIST 문서 한국어 요약

이 문서는 KAIST PintOS Project 1: Threads 문서를 처음 읽는 사람이 과제의 요구사항을 빠르게 이해할 수 있도록 정리한 한국어 요약본이다. 원문을 그대로 번역한 문서가 아니라, 과제를 시작하기 전에 반드시 알아야 할 목표, 구현 요구사항, 주의점, FAQ의 핵심 의미를 학습용으로 풀어 쓴 문서다.

구현 세부 전략은 `week9_pintos_project1_implementation_guide.md`에서 다루고, 운영체제 배경 개념은 `week9_pintos_threads_concepts.md`에서 다룬다. 이 문서는 그 중간 위치에서 "KAIST 문서가 요구하는 것이 정확히 무엇인가"를 이해하는 데 집중한다.

## 1. Project 1의 전체 목표

Project 1은 PintOS가 이미 제공하는 최소한의 thread system을 확장하는 과제다. 처음부터 운영체제를 새로 만드는 것이 아니라, 이미 존재하는 thread 생성, 종료, 스케줄링, 동기화 도구 위에 필요한 기능을 추가한다.

주요 작업 범위는 다음과 같다.

- `threads` 디렉터리의 thread scheduler와 synchronization 관련 코드
- `include/threads` 디렉터리의 thread, lock, semaphore 관련 헤더
- `devices/timer.c`의 timer sleep 관련 코드
- 필요시 fixed-point arithmetic을 위한 보조 헤더

과제의 핵심은 세 가지다.

1. Busy waiting 없이 thread를 재우고 깨우는 Alarm Clock을 만든다.
2. priority 기반 scheduling과 priority donation을 구현한다.
3. 4.4BSD 스타일의 MLFQS scheduler를 구현한다.

이 과제에서 중요한 것은 "테스트가 통과하는 코드"만이 아니다. 왜 thread가 ready, running, blocked 상태를 오가는지, 왜 interrupt를 끄는 범위를 최소화해야 하는지, 왜 lock donation이 필요한지, 왜 kernel에서는 floating-point를 쓰지 않는지를 이해해야 한다.

## 2. 먼저 이해해야 할 기본 thread system

PintOS의 thread는 하나의 실행 흐름이다. `thread_create()`를 호출하면 새 실행 context가 만들어지고, 그 thread가 처음 스케줄되면 전달받은 함수의 처음부터 실행된다. 함수가 끝나면 해당 thread도 종료된다. 이런 점에서 각 kernel thread는 PintOS 안에서 실행되는 작은 프로그램처럼 볼 수 있다.

어느 순간 CPU에서 실제로 실행되는 thread는 하나뿐이다. 나머지 thread는 실행 가능한 상태로 기다리거나, 어떤 사건이 일어나기를 기다리며 blocked 상태에 있다. scheduler는 다음에 어떤 thread를 실행할지 결정한다. 실행할 thread가 없으면 idle thread가 실행된다.

처음 코드를 읽을 때 특히 봐야 할 흐름은 다음과 같다.

- thread가 생성되는 흐름: `thread_create()`
- 현재 thread가 CPU를 양보하는 흐름: `thread_yield()`
- thread가 기다리기 위해 멈추는 흐름: `thread_block()`
- blocked thread가 다시 ready 상태가 되는 흐름: `thread_unblock()`
- 다음 실행 thread를 고르는 흐름: `schedule()`, `next_thread_to_run()`

KAIST 문서는 context switch 자체의 assembly 수준 동작을 반드시 깊게 이해할 필요는 없다고 안내한다. 다만 `schedule()` 전후로 현재 thread의 상태가 어떻게 바뀌고, ready list에 어떤 thread가 들어가고 나오는지는 추적할 수 있어야 한다.

## 3. stack 크기 제한과 메모리 사용 주의

PintOS의 각 kernel thread는 작은 고정 크기의 stack을 가진다. 대략 4KB 안쪽의 제한된 공간이므로, 큰 배열이나 큰 구조체를 지역 변수로 만들면 stack overflow가 발생할 수 있다.

예를 들어 thread 함수 안에서 큰 배열을 지역 변수로 선언하는 방식은 위험하다. 이런 코드는 컴파일은 될 수 있지만, 실행 중 이해하기 어려운 kernel panic이나 이상 동작으로 이어질 수 있다.

큰 데이터가 필요하면 다음 대안을 고려해야 한다.

- 전역 또는 static 데이터로 둘 수 있는지 검토한다.
- page allocator를 사용할 수 있는지 검토한다.
- block allocator 또는 `malloc()` 계열을 사용할 수 있는지 검토한다.

Project 1에서는 일반적으로 큰 동적 자료구조가 많이 필요하지 않다. list element와 thread field를 적절히 추가하는 정도로 해결할 수 있는 경우가 많다.

## 4. synchronization을 어떻게 바라봐야 하는가

Project 1의 많은 문제는 동기화 문제다. ready list, sleep list, semaphore waiters, lock holder, donation 정보처럼 여러 thread 또는 interrupt handler가 접근할 수 있는 상태를 안전하게 다루어야 한다.

가장 단순한 방법은 interrupt를 꺼서 방해받지 않는 구간을 만드는 것이다. 하지만 KAIST 문서는 모든 동기화 문제를 interrupt disable로 해결하지 말라고 강조한다. interrupt를 오래 꺼두면 timer tick이나 입력 이벤트 처리가 늦어지고, 전체 시스템 반응성이 나빠진다.

원칙은 다음과 같다.

- 일반 thread 사이의 동기화는 semaphore, lock, condition variable을 우선 사용한다.
- interrupt handler와 thread가 공유하는 데이터는 lock을 쓸 수 없으므로 interrupt disable로 보호한다.
- interrupt를 끄는 구간은 가능한 짧게 유지한다.
- tight loop에서 `thread_yield()`를 반복하는 busy waiting은 제출 코드에 남기면 안 된다.

특히 Alarm Clock과 MLFQS는 timer interrupt handler와 thread 상태를 함께 다루므로 interrupt 관련 설계를 조심해야 한다.

## 5. Alarm Clock 요구사항

Alarm Clock의 목표는 `timer_sleep()`을 busy waiting 없이 다시 구현하는 것이다.

기본 구현은 시간이 지났는지 반복해서 확인하면서 `thread_yield()`를 호출하는 방식이다. 이 방식은 CPU를 실제 작업에 쓰지 못하고 계속 낭비하므로 busy waiting이다. 과제에서는 잠들어야 하는 thread를 blocked 상태로 만들고, 지정된 tick이 지난 뒤 timer interrupt에서 깨워 ready list로 되돌리는 방식으로 바꾸어야 한다.

핵심 요구사항은 다음과 같다.

- `timer_sleep(int64_t ticks)`는 최소 `ticks` tick이 지난 뒤 현재 thread가 다시 실행 가능해지도록 해야 한다.
- 정확히 그 tick에 즉시 실행될 필요는 없다. 중요한 것은 충분한 시간이 지난 뒤 ready queue에 들어가는 것이다.
- `ticks`의 단위는 millisecond가 아니라 timer tick이다.
- `TIMER_FREQ`는 기본적으로 초당 100 tick이며, 테스트 안정성을 위해 바꾸지 않는 것이 좋다.
- `timer_msleep()`, `timer_usleep()`, `timer_nsleep()`은 필요하면 내부적으로 `timer_sleep()`을 사용하므로 보통 수정할 필요가 없다.

구현 관점에서 이해해야 할 상태 전이는 다음과 같다.

1. 현재 thread가 `timer_sleep()`을 호출한다.
2. 깨워야 할 tick을 계산한다.
3. 현재 thread를 sleep list에 넣고 blocked 상태로 만든다.
4. timer interrupt가 발생할 때마다 sleep list에서 깨어날 thread를 확인한다.
5. 깨어날 시간이 된 thread를 `thread_unblock()`하여 ready 상태로 보낸다.

여기서 중요한 점은 timer interrupt 안에서는 오래 걸리는 작업이나 sleep 가능한 작업을 하면 안 된다는 것이다. timer interrupt handler는 짧고 결정적으로 끝나야 한다.

## 6. Priority Scheduling 요구사항

Priority Scheduling은 ready 상태의 thread 중 priority가 가장 높은 thread를 먼저 실행하도록 만드는 기능이다. PintOS의 priority 값은 `PRI_MIN` 0부터 `PRI_MAX` 63까지이며, 숫자가 클수록 높은 priority다. 기본 priority는 `PRI_DEFAULT` 31이다.

핵심 요구사항은 다음과 같다.

- 높은 priority thread가 ready list에 들어오면 현재 thread보다 우선 실행될 수 있어야 한다.
- 새로 ready가 된 thread의 priority가 현재 running thread보다 높으면 즉시 CPU를 양보해야 한다.
- lock, semaphore, condition variable에서 기다리는 thread 중 가장 높은 priority thread가 먼저 깨어나야 한다.
- thread는 자기 priority를 올리거나 낮출 수 있다.
- 현재 thread가 priority를 낮춰 더 이상 가장 높은 priority가 아니게 되면 즉시 yield해야 한다.

이 요구사항은 단순히 `next_thread_to_run()`에서 높은 priority를 고르는 것만으로 끝나지 않는다. ready list에 thread가 들어가는 모든 지점에서 preemption을 고려해야 한다.

대표적으로 다음 상황을 확인해야 한다.

- `thread_create()`로 더 높은 priority thread가 생성된 직후
- `thread_unblock()`으로 더 높은 priority thread가 ready 상태가 된 직후
- `sema_up()`으로 더 높은 priority waiter가 깨어난 직후
- `lock_release()`로 donation이 해제되어 현재 thread의 effective priority가 낮아진 직후
- `thread_set_priority()`로 현재 thread의 base priority가 낮아진 직후

## 7. Priority Donation이 필요한 이유

Strict priority scheduling에서는 priority inversion 문제가 발생할 수 있다. 예를 들어 high priority thread H가 low priority thread L이 가진 lock을 기다리고 있다고 하자. 이때 medium priority thread M이 ready 상태라면, scheduler는 M을 L보다 먼저 실행한다. 그러면 L은 lock을 release할 기회를 얻지 못하고, 결과적으로 H도 계속 기다리게 된다.

Priority Donation은 이 문제를 완화하기 위한 장치다. H가 L이 가진 lock을 기다리면, H는 자신의 높은 priority를 L에게 일시적으로 빌려준다. 그러면 L이 빨리 실행되어 lock을 release할 수 있고, H도 다시 진행할 수 있다.

중요한 규칙은 다음과 같다.

- priority donation은 lock에 대해서만 구현하면 된다.
- semaphore와 condition variable에는 donation을 구현할 필요는 없다.
- 하지만 semaphore와 condition variable에서도 priority scheduling 자체는 지켜야 한다.
- donation은 더하는 것이 아니다. priority 5가 priority 3에게 donate하면 결과는 8이 아니라 5다.
- 여러 thread가 한 thread에게 donate하면 가장 높은 donated priority가 effective priority가 된다.
- nested donation을 처리해야 한다.

Nested donation은 다음과 같은 상황이다.

1. H가 M이 가진 lock을 기다린다.
2. M은 다시 L이 가진 lock을 기다린다.
3. 그러면 H의 priority가 M에게 전달되고, 다시 L에게도 전달되어야 한다.

KAIST 문서는 필요하다면 nested donation 깊이에 합리적인 제한을 둘 수 있다고 설명한다. 예를 들어 8단계 제한은 KAIST 문서가 예시로 든 합리적인 제한이다.

## 8. `thread_set_priority()`와 donation의 관계

Donation을 구현하면 priority는 두 종류로 나누어 생각해야 한다.

- base priority: thread가 원래 가지고 있는 priority
- effective priority: donation까지 반영한 실제 scheduling priority

`thread_set_priority()`는 base priority를 바꾸는 함수로 보는 것이 자연스럽다. donation을 받고 있는 thread가 `thread_set_priority()`를 호출하더라도, effective priority는 base priority와 donated priority 중 큰 값이어야 한다.

예를 들어 base priority를 10으로 낮췄더라도, 현재 50을 donation 받고 있다면 effective priority는 50이어야 한다. 이후 lock을 release해서 donation이 사라지면 그때 base priority인 10으로 돌아간다.

`thread_get_priority()`는 donation이 있는 경우 effective priority를 반환해야 한다.

## 9. Advanced Scheduler, 즉 MLFQS 요구사항

Advanced Scheduler는 4.4BSD scheduler와 유사한 multilevel feedback queue scheduler를 구현하는 부분이다. 목표는 여러 종류의 작업이 섞여 있을 때 평균 response time을 줄이는 것이다.

Priority scheduler와 마찬가지로 thread priority를 기준으로 실행할 thread를 고르지만, 중요한 차이가 있다.

- MLFQS에서는 priority donation을 사용하지 않는다.
- `-mlfqs` kernel option으로 MLFQS를 켤 수 있어야 한다.
- 기본값은 priority scheduler이고, 옵션이 주어졌을 때만 MLFQS가 활성화된다.
- MLFQS가 켜져 있으면 `thread_create()`의 priority 인자는 무시된다.
- MLFQS가 켜져 있으면 `thread_set_priority()` 호출도 직접 priority를 바꾸지 않는다.
- `thread_get_priority()`는 scheduler가 계산한 현재 priority를 반환해야 한다.

추가로 테스트 프로그램이 사용할 다음 함수들도 구현해야 한다.

- `thread_get_nice()`: 현재 thread의 nice 값을 반환한다.
- `thread_set_nice(int nice)`: 현재 thread의 nice 값을 바꾸고, 새 nice 기준으로 priority를 다시 계산하며, 필요하면 yield한다.
- `thread_get_recent_cpu()`: 현재 thread의 `recent_cpu` 값에 100을 곱해 반올림한 정수를 반환한다.
- `thread_get_load_avg()`: 시스템 `load_avg` 값에 100을 곱해 반올림한 정수를 반환한다.

MLFQS는 thread가 직접 priority를 정하는 방식이 아니라, scheduler가 CPU 사용 패턴과 nice 값을 보고 priority를 계속 다시 계산하는 방식이다.

## 10. MLFQS의 핵심 개념: nice, recent_cpu, load_avg

MLFQS를 이해하려면 세 값을 구분해야 한다.

### nice

`nice`는 thread가 다른 thread에게 얼마나 CPU를 양보하려는지를 나타내는 값이다. 값의 범위는 -20부터 20까지다.

- nice가 0이면 기본 상태다.
- nice가 양수이면 priority가 낮아져 CPU를 덜 받는 방향으로 간다.
- nice가 음수이면 priority가 높아져 CPU를 더 받는 방향으로 간다.
- 새 thread는 부모 thread의 nice 값을 상속한다.

### recent_cpu

`recent_cpu`는 해당 thread가 최근에 CPU를 얼마나 사용했는지를 나타내는 추정값이다. timer tick마다 running thread의 `recent_cpu`가 증가한다. 단, idle thread는 제외한다.

`recent_cpu`는 단순 누적값이 아니다. 시간이 지나면 과거 CPU 사용량의 영향이 점점 줄어드는 moving average 방식으로 갱신된다. 그래서 오래전에 CPU를 많이 쓴 thread보다 최근에 CPU를 많이 쓴 thread의 priority가 더 강하게 낮아진다.

negative nice 값을 가진 thread는 `recent_cpu`가 음수가 될 수 있다. 이 값을 0으로 clamp하면 안 된다.

### load_avg

`load_avg`는 시스템 전체 관점에서 최근 1분 동안 평균적으로 몇 개의 thread가 CPU를 얻기 위해 경쟁했는지를 추정하는 값이다.

계산할 때 idle thread는 제외한다. ready 상태 thread 수와 현재 running thread를 포함하되, 현재 running thread가 idle thread이면 포함하지 않는다.

## 11. MLFQS 공식 정리

MLFQS에서 priority는 다음 공식으로 계산된다.

```text
priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
```

계산 결과는 정수로 내림 처리하고, 최종 priority는 `PRI_MIN`부터 `PRI_MAX` 범위 안으로 clamp해야 한다.

`recent_cpu`는 매초 다음 공식으로 모든 thread에 대해 갱신된다.

```text
recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
```

`load_avg`는 매초 다음 공식으로 갱신된다.

```text
load_avg = (59 / 60) * load_avg + (1 / 60) * ready_threads
```

갱신 주기도 중요하다.

- 매 tick: running thread의 `recent_cpu`를 1 증가시킨다. 단, idle thread는 제외한다.
- 매 4 tick: 모든 thread의 priority를 다시 계산한다.
- 매 1초: `load_avg`와 모든 thread의 `recent_cpu`를 다시 계산한다.

1초 단위 갱신에서는 먼저 `load_avg`를 갱신하고, 갱신된 `load_avg`를 사용해 모든 thread의 `recent_cpu`를 다시 계산하는 순서로 생각하면 안전하다. 그 뒤 해당 tick이 4의 배수라면 priority 재계산까지 이어진다.

KAIST 문서는 일부 테스트가 정확한 갱신 시점을 가정한다고 설명한다. 특히 1초 단위 갱신은 `timer_ticks() % TIMER_FREQ == 0`인 시점에 정확히 수행해야 한다. 또한 timer tick 증가를 본 일반 kernel thread가 낡은 scheduler 값을 읽지 않도록, 이러한 scheduler 값 갱신은 일반 thread가 다시 실행되기 전에 끝나야 한다.

## 12. kernel에서 fixed-point arithmetic이 필요한 이유

MLFQS 공식에는 실수 계산이 필요해 보이는 값이 있다. 하지만 PintOS kernel에서는 floating-point arithmetic을 사용하지 않는다. kernel에서 floating-point를 사용하려면 CPU의 floating-point register 상태를 context switch마다 저장하고 복원해야 하므로 복잡성과 비용이 커진다.

그래서 실수를 정수로 흉내 내는 fixed-point arithmetic을 사용한다. KAIST 문서는 17.14 fixed-point 형식을 예로 든다. 이는 정수 하나를 사용하되 하위 14비트를 소수부처럼 해석하는 방식이다.

구현 시 주의할 점은 다음과 같다.

- fixed-point 값끼리 곱할 때는 overflow를 피하기 위해 64-bit 정수 연산을 사용한다.
- fixed-point 값끼리 나눌 때도 64-bit 정수 연산을 사용하는 것이 안전하다.
- 음수 값의 반올림은 양수와 다르게 처리해야 한다.
- shift 연산은 음수와 precedence 문제 때문에 조심해야 하며, 곱셈과 나눗셈으로 구현하는 편이 명확하다.

Project 1에서는 fixed-point helper를 따로 만들어 두면 MLFQS 구현이 훨씬 안정적이다.

## 13. FAQ 핵심 정리

### 얼마나 많은 코드를 작성해야 하는가

KAIST FAQ는 reference solution의 변경 규모를 예시로 보여준다. 이는 정답의 유일한 형태가 아니라 참고용이다. 핵심은 대략 `timer.c`, `thread.c`, `thread.h`, `synch.c`, `synch.h`, fixed-point helper 정도가 주요 변경 대상이라는 점이다.

### 새 `.h` 파일을 추가하면 Makefile을 수정해야 하는가

새 `.c` 파일을 추가하면 각 디렉터리의 `targets.mk`를 수정해야 할 수 있다. 반면 새 `.h` 파일만 추가하는 경우 보통 Makefile 수정은 필요 없다. fixed-point helper를 header-only로 만들면 이 점에서 단순하다.

### `no previous prototype` warning은 무엇인가

non-static 함수를 선언 없이 정의했다는 뜻이다. 다른 파일에서 쓰는 함수라면 header에 prototype을 두어야 하고, 해당 파일 내부에서만 쓰는 helper라면 `static`으로 만드는 것이 좋다.

### timer interrupt 간격과 time slice는 바꾸어도 되는가

`TIMER_FREQ` 기본값은 100Hz이고, `TIME_SLICE` 기본값은 4 tick이다. 테스트는 이 값을 전제로 작성되어 있으므로 바꾸지 않는 것이 좋다.

### `pass()`에서 test failure가 난 것처럼 보이는 이유

backtrace에서 `pass()`가 보인다고 해서 `pass()`가 실패를 일으킨 것은 아닐 수 있다. `fail()`이 `PANIC()`을 호출한 뒤 함수가 반환되지 않는다는 컴파일러 최적화 특성 때문에 인접한 함수 주소가 보일 수 있다. panic의 실제 원인은 backtrace와 테스트 코드를 함께 봐야 한다.

### `schedule()` 이후 interrupt는 어떻게 다시 켜지는가

`schedule()`로 들어가는 경로는 interrupt를 끈 상태다. 이후 어떤 경로로 schedule이 호출되었는지에 따라 새 thread 또는 호출자가 interrupt level을 복구한다. 새로 만들어진 thread는 처음 실행될 때 `kernel_thread()` 쪽에서 interrupt를 켠다.

이 내용은 구현 중 `intr_disable()`과 `intr_set_level()`의 짝을 맞추는 것이 왜 중요한지 이해하는 데 도움이 된다.

### timer 값 overflow를 걱정해야 하는가

걱정하지 않아도 된다. timer 값은 signed 64-bit이고, 초당 100 tick 기준으로 사실상 과제 범위에서 overflow를 고려할 필요가 없다.

### strict priority scheduling은 starvation을 만들지 않는가

만들 수 있다. strict priority scheduling에서는 높은 priority thread가 계속 runnable이면 낮은 priority thread가 CPU를 얻지 못할 수 있다. MLFQS는 이런 문제를 완화하기 위해 priority를 동적으로 조정한다.

### lock release 후 어떤 thread가 실행되어야 하는가

lock을 release하면 그 lock을 기다리던 thread 중 highest priority thread가 unblock되어 ready list로 들어가야 한다. 이후 scheduler는 ready list와 현재 thread를 비교해 가장 높은 priority thread가 실행되도록 해야 한다.

### highest-priority thread가 yield하면 계속 실행될 수 있는가

같은 priority의 다른 ready thread가 없다면 계속 실행될 수 있다. 같은 highest priority thread가 여러 개라면 round-robin 순서로 돌아가야 한다.

### donating thread의 priority도 바뀌는가

아니다. donation은 donee, 즉 priority를 받는 thread의 effective priority를 바꾼다. donor 자신의 priority는 그대로다.

### ready queue나 blocked 상태에서도 priority가 바뀔 수 있는가

가능하다. ready 상태의 low priority thread가 lock을 들고 있고, high priority thread가 그 lock을 기다리면 ready 상태의 thread도 donation을 받을 수 있다. blocked 상태의 thread도 자신이 가진 lock 때문에 donation을 받을 수 있다.

따라서 priority 변경은 running thread에서만 일어난다고 가정하면 안 된다.

### ready list에 추가된 thread가 즉시 preempt할 수 있는가

그렇다. 높은 priority thread가 ready 상태가 되면 다음 timer interrupt까지 기다리지 않고 즉시 현재 CPU를 양보해야 한다. 이 요구사항은 `priority-preempt` 계열 테스트에서 중요하다.

### priority donation과 MLFQS는 함께 고려해야 하는가

KAIST FAQ는 priority donation과 advanced scheduler를 동시에 테스트하지 않는다고 설명한다. 따라서 `thread_mlfqs`가 true일 때는 donation 로직을 비활성화하는 방식으로 분리하는 것이 자연스럽다.

### 64개 ready queue를 반드시 만들어야 하는가

반드시 그럴 필요는 없다. 원문은 64개 priority queue 모델로 설명하지만, 외부 동작이 같다면 하나의 ready list를 priority 순서로 관리하는 방식도 가능하다. 중요한 것은 구현 방식이 아니라 관찰 가능한 scheduler 동작이다.

### MLFQS 테스트가 이해하기 어렵게 실패할 때

먼저 실패한 테스트 파일의 주석을 읽어야 한다. PintOS 테스트는 대개 파일 상단에 테스트 의도와 기대 결과를 설명한다. 그 다음 fixed-point arithmetic, 갱신 주기, timer interrupt 안에서 수행하는 작업량을 점검해야 한다.

timer interrupt handler가 너무 오래 걸리면, interrupt에 걸린 thread가 실제보다 더 많은 CPU를 쓴 것처럼 계산될 수 있다. 이는 `recent_cpu`와 `load_avg`에 영향을 주고, 결국 scheduling 결과를 바꿀 수 있다.

## 14. 구현 전에 반드시 확인할 체크포인트

Project 1을 시작하기 전에 다음 질문에 답할 수 있어야 한다.

- 현재 thread가 running, ready, blocked 중 어느 상태인지 설명할 수 있는가?
- `thread_block()`과 `thread_unblock()`이 각각 언제 호출되어야 하는가?
- timer interrupt 안에서 해도 되는 일과 하면 안 되는 일을 구분할 수 있는가?
- busy waiting과 blocking의 차이를 설명할 수 있는가?
- ready list가 항상 priority 순서를 만족해야 하는지, 아니면 선택 시점에 max를 찾을 것인지 결정했는가?
- semaphore waiters와 condition waiters에서도 highest priority waiter가 먼저 깨어나도록 설계했는가?
- lock donation, multiple donation, nested donation, donation 회수를 구분해서 설명할 수 있는가?
- base priority와 effective priority를 분리해서 생각하고 있는가?
- MLFQS가 켜졌을 때 donation과 manual priority 설정을 배제하고 있는가?
- `recent_cpu`, `load_avg`, priority 갱신 시점이 KAIST 문서의 요구와 일치하는가?
- fixed-point 연산에서 overflow와 음수 반올림을 고려했는가?

## 15. 읽는 순서 추천

처음부터 모든 세부 구현을 동시에 잡으려고 하면 혼란스럽다. 다음 순서로 읽고 구현하는 것이 좋다.

1. KAIST Introduction을 읽고 Project 1의 범위와 synchronization 원칙을 이해한다.
2. `thread.c`, `thread.h`, `synch.c`, `timer.c`의 기본 흐름을 훑는다.
3. Alarm Clock 문서를 읽고 busy waiting 제거를 먼저 구현한다.
4. Priority Scheduling 문서를 읽고 ready list와 preemption을 구현한다.
5. Priority Donation 요구사항을 읽고 lock 중심으로 donation을 구현한다.
6. Advanced Scheduler 문서를 읽고 MLFQS를 별도 모드로 구현한다.
7. FAQ를 보면서 edge case와 테스트 실패 원인을 점검한다.

## 16. 원문 링크

- KAIST Project 1 Introduction: https://casys-kaist.github.io/pintos-kaist/project1/introduction.html
- KAIST Project 1 Alarm Clock: https://casys-kaist.github.io/pintos-kaist/project1/alarm_clock.html
- KAIST Project 1 Priority Scheduling: https://casys-kaist.github.io/pintos-kaist/project1/priority_scheduling.html
- KAIST Project 1 Advanced Scheduler: https://casys-kaist.github.io/pintos-kaist/project1/advanced_scheduler.html
- KAIST Project 1 FAQ: https://casys-kaist.github.io/pintos-kaist/project1/FAQ.html
