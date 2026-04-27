# 9주차 PintOS Project 1 구현 및 테스트 지침서

이 문서는 PintOS `Project 1: Threads`를 실제로 구현하고 테스트를 통과하기 위한 작업 지침서다. 운영체제 개념과 CS 원리는 `week9_pintos_threads_concepts.md`에서 다루며, 이 문서는 "어떤 순서로 코드를 고치고, 어떤 테스트로 검증하고, 실패하면 어디를 의심할지"에 초점을 둔다.

주의할 점은 이 문서가 정답 코드를 제공하지 않는다는 것이다. PintOS는 팀 프로젝트이고 테스트가 구현의 세부 오류를 잘 드러내므로, 직접 설계하고 작은 단위로 검증하는 방식이 가장 안전하다.

문서 역할을 명확히 구분하면 다음과 같다.

- 개념 학습 노트: 스레드, 스케줄링, 인터럽트, 동기화, donation, MLFQS가 왜 필요한지 이해한다.
- 구현 지침서: 로컬 PintOS 코드에서 어떤 파일과 함수가 그 개념을 담당하는지 찾고, 테스트를 통과하도록 단계적으로 수정한다.

## 1. 개발 환경 확인

로컬 PintOS 포맷은 Docker와 VS Code DevContainer를 기준으로 한다. 프로젝트 루트는 `pintos` 폴더를 포함하는 Docker 실습 폴더이며, VS Code에서 `Dev Containers: Reopen in Container`로 컨테이너 안에서 열어 작업한다.

환경 관련 핵심 사항은 다음과 같다.

- 실습 환경은 x86-64 기반 KAIST PintOS다.
- VS Code의 F5 통합 디버깅은 제공되지 않는다.
- 디버깅이 필요하면 `gdb`를 사용한다.
- VS Code 터미널을 열면 PintOS 활성화 스크립트가 자동 실행되도록 구성되어 있다.
- 9주차부터 12주차까지 같은 환경을 사용하므로, 임시 변경이나 실험 코드는 Git branch로 분리하는 것이 안전하다.

처음 시작할 때 확인할 명령은 다음과 같다.

```bash
cd pintos/threads
make clean
make
make check
```

전체 테스트가 오래 걸리면 개별 테스트부터 실행한다. PintOS 테스트 실행 방식은 배포판에 따라 약간 다를 수 있으므로, `pintos/tests/threads`와 `Make.tests`를 함께 확인한다.

## 2. 주로 수정하게 될 파일

Project 1에서 주로 건드리는 파일은 다음 정도로 제한하는 것이 좋다.

| 파일 | 역할 |
|---|---|
| `devices/timer.c` | alarm clock, sleeping list, timer interrupt wakeup |
| `threads/thread.c` | scheduler, ready_list, priority, donation, MLFQS |
| `threads/synch.c` | semaphore, lock, condition variable의 priority-aware 동작 |
| `include/threads/thread.h` | `struct thread`에 필요한 필드 추가 |
| `include/threads/synch.h` | lock에 donation 추적용 필드가 필요하면 추가 |
| `include/lib/kernel/list.h`, `lib/kernel/list.c` | `list_insert_ordered()`, `list_max()`, `list_remove()` 등 리스트 API 확인 |

가능하면 수정 범위를 위 파일들로 유지한다. Project 1을 위해 관련 없는 하위 시스템을 고치기 시작하면 원인 추적이 어려워진다.

## 3. 권장 구현 순서

처음부터 donation과 MLFQS까지 한 번에 구현하면 실패 원인을 찾기 어렵다. 다음 순서로 작은 목표를 잡는 것이 좋다.

1. Alarm Clock 구현
2. Priority Scheduling의 ready queue와 preemption 구현
3. Semaphore와 condition variable의 priority wakeup 구현
4. Priority Donation 구현
5. MLFQS 구현
6. 전체 테스트 반복 실행 및 회귀 확인

각 단계가 통과한 뒤 다음 단계로 넘어가야 한다. 특히 donation 구현은 priority scheduling의 기본 불변식이 맞아야 정상적으로 동작한다.

## 4. Stage 1: Alarm Clock

### 목표

`timer_sleep()`에서 busy waiting을 제거한다. 일정 tick 동안 기다리는 스레드는 ready_list에서 빠져 blocked 상태가 되어야 하며, wakeup tick이 되면 timer interrupt에서 다시 ready 상태로 돌아와야 한다.

### 필요한 설계

대체로 다음 필드와 자료구조가 필요하다.

- 전역 sleeping list
- 각 스레드의 wakeup tick 필드
- wakeup tick 기준 비교 함수

`struct thread`에 예를 들어 `int64_t wakeup_tick` 같은 필드를 추가할 수 있다. sleeping list는 `thread.c`에 둘 수도 있고 `timer.c`에 둘 수도 있다. 어느 쪽이든 timer interrupt에서 접근할 수 있어야 한다.

### 구현 흐름

`timer_sleep(ticks)`의 의사 흐름은 다음과 같다.

```text
if ticks <= 0:
    return

old_level = intr_disable()
현재 tick을 읽고 wakeup tick을 현재 tick + ticks로 계산한다.
현재 스레드의 wakeup tick 필드에 기록한다.
sleeping list에 현재 스레드를 넣는다.
현재 스레드를 block한다.
intr_set_level(old_level)
```

`timer_sleep()` 진입 시점에는 기존 코드처럼 interrupt가 켜져 있어야 한다고 가정해도 된다. 하지만 tick 읽기, wakeup tick 기록, sleeping list 삽입, `thread_block()`은 하나의 임계구역으로 묶는 편이 안전하다. wakeup tick 계산, list 삽입, block 사이에 timer interrupt가 끼어들면 목표 tick을 놓쳐 의도보다 더 오래 잘 수 있다.

`timer_interrupt()`의 의사 흐름은 다음과 같다.

```text
ticks를 증가시킨다.
sleeping list에서 wakeup tick이 현재 ticks 이하인 스레드를 찾는다.
해당 스레드들을 sleeping list에서 제거하고 thread_unblock() 한다.
thread_tick()을 호출한다.
```

sleeping list를 wakeup tick 기준으로 정렬해 두면 매 tick마다 앞쪽만 확인하면 된다. 전체 순회 방식도 테스트 규모에서는 가능하지만, 정렬 방식이 더 명확하다.

### 주의점

- `timer_sleep(0)`과 음수 ticks는 block하지 않고 바로 return해야 한다.
- `timer_interrupt()`는 interrupt context이므로 현재 스레드를 재우면 안 된다.
- sleeping list 조작과 `thread_block()`은 원자적으로 처리되어야 한다.
- `thread_block()`은 interrupt가 꺼진 상태에서 호출되어야 한다.
- wakeup 시점에 `thread_unblock()`을 호출한다고 해서 즉시 실행되는 것은 아니다. ready 상태가 될 뿐이며, interrupt context에서 더 높은 priority 스레드를 깨웠다면 return 시점 선점을 위해 `intr_yield_on_return()`이 필요할 수 있다.

### 우선 확인할 테스트

- `alarm-single`
- `alarm-multiple`
- `alarm-simultaneous`
- `alarm-zero`
- `alarm-negative`
- `alarm-priority`

`alarm-priority`는 alarm 자체뿐 아니라 priority scheduling과도 연결된다. alarm 기본 테스트를 먼저 통과한 뒤 priority 구현 후 다시 확인한다.

## 5. Stage 2: Priority Scheduling

### 목표

ready 상태의 스레드 중 priority가 가장 높은 스레드가 실행되도록 만든다. 더 높은 priority 스레드가 ready 상태가 되면 현재 스레드는 CPU를 양보해야 한다.

### 수정 후보

- `thread_unblock()`
- `thread_yield()`
- `thread_create()`
- `next_thread_to_run()`
- `thread_set_priority()`
- 비교 함수: thread priority 비교

### ready_list 관리 방식

두 가지 방식 중 하나를 선택한다.

| 방식 | 장점 | 단점 |
|---|---|---|
| 삽입 시 정렬 | `next_thread_to_run()`이 단순하다 | 모든 삽입 지점에서 정렬 삽입을 지켜야 한다 |
| 선택 시 최댓값 탐색 | 삽입은 단순하다 | 매 schedule마다 O(n) 탐색이 필요하다 |

테스트 규모에서는 둘 다 가능하다. 다만 어떤 방식을 선택하든 ready_list를 다루는 모든 함수에서 일관성을 유지해야 한다.

같은 priority 사이의 FIFO 순서도 테스트 대상이다. 정렬 삽입 방식을 쓴다면 비교 함수에서 "새 thread priority가 기존 thread priority보다 클 때만 앞에 넣는다"는 식으로 안정성을 유지해야 한다. `>=` 기준으로 같은 priority thread를 앞에 끼워 넣으면 `priority-fifo`가 깨질 수 있다.

### 선점이 필요한 시점

다음 상황에서 현재 스레드가 CPU를 양보해야 할 수 있다.

- `thread_create()`로 더 높은 priority 스레드가 생성된 직후
- `thread_unblock()`으로 더 높은 priority 스레드가 ready 상태가 된 직후
- `thread_set_priority()`로 현재 스레드의 priority가 낮아진 직후
- `sema_up()`이나 `lock_release()`로 더 높은 priority waiter가 깨어난 직후

일반 thread context에서는 `thread_yield()`를 사용할 수 있다. interrupt context에서는 직접 yield하지 말고 `intr_yield_on_return()`을 사용해야 한다.

로컬 기본 코드의 `thread_unblock()` 주석은 이 함수가 running thread를 직접 preempt하지 않는다고 설명한다. 따라서 구현할 때는 `thread_unblock()` 내부에서 무조건 `thread_yield()`를 호출하는 방식보다, 호출 맥락을 구분하는 helper를 두는 편이 안전하다. 일반 thread context에서는 `thread_yield()`, interrupt context에서는 `intr_yield_on_return()`을 사용하고, interrupt가 꺼진 상태에서 원자적 작업을 이어가야 하는 호출자는 작업을 마친 뒤 선점 여부를 판단하게 설계한다.

### `thread_set_priority()`의 기본 동작

Donation을 구현하기 전이라면 `thread_set_priority(new_priority)`는 현재 스레드의 priority를 바꾸고, ready_list에 자신보다 높은 priority 스레드가 있으면 yield하면 된다.

Donation 구현 후에는 달라진다. 현재 priority는 donation이 반영된 effective priority일 수 있으므로 base priority를 따로 저장해야 한다. 이 부분은 Stage 4에서 다시 정리한다.

### 우선 확인할 테스트

- `priority-change`
- `priority-preempt`
- `priority-fifo`

이 테스트들이 흔들리면 donation으로 넘어가면 안 된다. donation 테스트 실패가 priority 기본 로직 문제인지 donation 문제인지 분리할 수 없기 때문이다.

## 6. Stage 3: Synchronization Waiters의 Priority 처리

### 목표

ready_list뿐 아니라 semaphore와 condition variable에서 기다리는 스레드도 priority 순서로 깨어나야 한다.

### Semaphore

`sema_down()`에서 waiters에 현재 스레드를 넣을 때 priority 순서로 넣거나, `sema_up()`에서 waiters 중 priority가 가장 높은 스레드를 찾아 깨운다. 다만 donation이 구현된 뒤에는 blocked 상태의 waiter priority가 나중에 바뀔 수 있으므로, `sema_up()` 시점에 `list_max()`를 사용하거나 waiters를 다시 정렬해 effective priority 기준 최고 waiter를 고르는 방식이 더 안전하다.

동일 priority waiter 사이에서는 FIFO 순서를 유지하는 편이 좋다. 테스트가 직접 모든 semaphore 동률 상황을 강하게 검증하지 않더라도, ready_list의 `priority-fifo` 관점과 일관되고 디버깅할 때 실행 순서를 예측하기 쉽다.

중요한 점은 `sema_up()` 후 더 높은 priority 스레드가 깨어났다면 현재 스레드가 yield해야 할 수 있다는 것이다. 단, `sema_up()`은 interrupt handler에서도 호출될 수 있으므로 context를 구분해야 한다.

의사 흐름은 다음과 같다.

```text
sema_up(sema):
    interrupt를 끈다.
    waiters가 비어 있지 않으면 highest priority waiter를 선택한다.
    선택한 waiter를 thread_unblock() 한다.
    sema->value를 증가시킨다.
    interrupt level을 복구한다.
    필요하면 yield 또는 yield_on_return 한다.
```

`sema->value++`와 unblock 순서는 구현 관점에서 신중해야 한다. 기존 PintOS 구조를 유지하되, waiters 선택 기준을 priority로 바꾸고 선점 판단을 추가하는 식으로 접근하는 것이 안전하다.

이 의사 흐름은 interrupt가 꺼진 상태에서 실행되고, `thread_unblock()`이 호출 즉시 context switch를 수행하지 않는다는 전제에서 성립한다. 따라서 waiter를 ready 상태로 바꾼 뒤 semaphore 값을 조정하고, interrupt level을 복구한 다음 현재 문맥에 맞게 yield 여부를 판단할 수 있다.

waiter 선택 기준은 donation이 반영된 effective priority여야 한다. semaphore 자체는 holder가 없으므로 donation을 새로 만들지는 않지만, 이미 lock donation을 받은 스레드가 semaphore에서 기다리는 경우에는 그 effective priority를 기준으로 먼저 깨워야 한다.

### Condition Variable

`cond_wait()`는 `struct semaphore_elem waiter`를 만들어 `cond->waiters`에 넣는다. 따라서 condition variable의 waiters는 thread list가 아니라 semaphore_elem list다.

`cond_signal()`에서는 다음 중 하나의 방식이 필요하다.

- `cond->waiters`를 semaphore_elem 내부 waiter의 priority 기준으로 정렬한다.
- signal 시점에 가장 높은 priority waiter를 가진 semaphore_elem을 찾는다.

로컬 기본 코드의 `struct semaphore_elem`에는 priority 필드가 없다. 가장 단순한 구현은 `semaphore_elem`에 priority snapshot 필드를 추가하고, `cond_wait()`에서 현재 thread의 effective priority를 기록한 뒤 `cond->waiters`를 이 값 기준으로 정렬하거나 `cond_signal()`에서 최댓값을 고르는 방식이다. 이 방식은 `priority-condvar` 테스트를 이해하고 구현하기 쉽다.

`priority-condvar`는 donation 전파 자체보다 condition variable waiters 중 높은 priority thread가 먼저 signal되는지를 확인하는 테스트에 가깝다. 따라서 먼저 wait/signal 순서를 priority 기준으로 맞춘 뒤, donation과 결합했을 때 snapshot 값이 stale해지는 문제를 별도로 점검하는 순서가 안전하다.

단, snapshot 값은 waiter의 effective priority가 나중에 donation으로 바뀌면 stale해질 수 있다. 더 일반적인 구현을 원한다면 signal 시점에 내부 semaphore waiters를 확인해 최신 priority를 비교할 수 있지만, `cond_wait()`가 `cond->waiters`에 `semaphore_elem`을 넣은 직후 아직 `sema_down()`에 들어가기 전인 짧은 타이밍에는 내부 waiters가 비어 있을 수 있다. 이 경우에도 signal은 해당 waiter의 semaphore value를 올려 저장될 수 있으므로, 단순히 내부 waiters가 비어 있다는 이유로 후보에서 제외하면 안 된다.

따라서 팀 구현에서는 기준을 하나로 정해야 한다. 실습 초반에는 priority snapshot 방식으로 시작해도 충분하다. 먼저 `priority-condvar`가 요구하는 wait/signal 순서를 맞추고, donation 구현 이후 snapshot 값이 stale해질 수 있는 경우를 별도 점검하는 순서가 안전하다. 더 정밀하게 만들려면 snapshot 갱신 전략이나 signal 시점 비교의 예외 처리를 추가한다. 핵심은 `cond_signal()`이 결과적으로 가장 높은 priority의 condition waiter를 깨우도록 일관된 기준을 유지하는 것이다.

### 우선 확인할 테스트

- `priority-sema`
- `priority-condvar`

이 두 테스트가 실패하면 donation 이전에 waiters 정렬과 wakeup 순서를 의심한다.

## 7. Stage 4: Priority Donation

### 목표

높은 priority 스레드가 낮은 priority 스레드가 보유한 lock을 기다릴 때 priority inversion을 방지한다. lock holder는 lock을 release할 때까지 높은 priority를 임시로 빌려받아야 한다.

### 추가 필드 설계

구현 방식은 다양하지만 보통 다음 정보가 필요하다.

| 위치 | 필드 예시 | 의미 |
|---|---|---|
| `struct thread` | `base_priority` | donation이 없는 원래 priority |
| `struct thread` | `waiting_lock` | 현재 acquire하려고 기다리는 lock |
| `struct thread` | `donations` 또는 held locks | 자신에게 들어온 donation 추적 |
| `struct lock` | `elem` | holder의 lock 목록에 넣기 위한 list element |
| `struct lock` | `max_priority` | 이 lock을 기다리는 waiter 중 최대 priority 캐시 |

반드시 위 이름을 쓸 필요는 없다. 중요한 것은 다음 질문에 답할 수 있어야 한다는 점이다.

- 이 스레드의 원래 priority는 무엇인가?
- 이 스레드의 현재 effective priority는 무엇인가?
- 이 스레드는 지금 어떤 lock을 기다리는가?
- 이 스레드가 release하는 lock 때문에 들어온 donation만 어떻게 제거할 것인가?

### lock acquire 흐름

`lock_acquire(lock)`에서는 `sema_down()`으로 block되기 전에 donation 정보를 설정해야 한다.

의사 흐름은 다음과 같다.

```text
현재 스레드 cur를 구한다.

if lock이 이미 holder를 가지고 있고 MLFQS가 꺼져 있다:
    cur->waiting_lock = lock
    lock holder에게 cur의 effective priority를 donation한다.
    holder가 또 다른 lock을 기다리고 있으면 chain을 따라 donation한다.

sema_down(&lock->semaphore)

cur->waiting_lock = NULL
lock->holder = cur
cur의 held lock 목록에 lock을 추가한다.
```

핵심은 donation이 block 전에 일어나야 한다는 것이다. block된 뒤에는 현재 스레드가 실행되지 않으므로 donation 전파 타이밍을 놓친다.

### lock release 흐름

`lock_release(lock)`에서는 해당 lock 때문에 들어온 donation만 제거해야 한다.

의사 흐름은 다음과 같다.

```text
현재 스레드 cur를 구한다.
cur의 held lock 목록에서 lock을 제거한다.
lock을 기다리던 스레드들의 donation 효과를 cur에서 제거한다.
cur의 effective priority를 다시 계산한다.
lock->holder = NULL
sema_up(&lock->semaphore)
필요하면 yield한다.
```

여기서 가장 흔한 버그는 donation을 전부 없애 버리는 것이다. 여러 lock을 가진 스레드가 하나의 lock만 release했다면, 다른 lock 때문에 들어온 donation은 계속 유지되어야 한다.

### `thread_set_priority()`와 donation

Donation이 구현된 뒤에는 `thread_set_priority(new_priority)`가 단순 대입이면 안 된다.

상황별로 생각해야 한다.

- donation이 없는 경우: base priority와 effective priority를 모두 새 값으로 바꾼다.
- donation을 받고 있는 경우: base priority만 새 값으로 바꾸고, effective priority는 donation 최댓값과 비교해 다시 계산한다.
- 새 base priority가 donation보다 높다면 effective priority도 base priority가 될 수 있다.
- priority 변경 후 자신보다 높은 ready thread가 있으면 yield한다.

이 로직이 틀리면 `priority-donate-lower`에서 자주 실패한다.

### nested donation

nested donation은 `waiting_lock`을 따라 전파하면 이해하기 쉽다.

```text
donor = 현재 스레드
lock = donor->waiting_lock

while lock이 있고 lock->holder가 있고 깊이 제한을 넘지 않았다:
    holder의 effective priority를 donor priority 이상으로 올린다.
    donor = holder
    lock = holder->waiting_lock
```

깊이 제한은 필수는 아니지만, 잘못된 포인터나 순환 구조로 무한 루프에 빠지는 것을 막는 방어 장치가 될 수 있다. PintOS 테스트에서는 보통 제한된 깊이의 chain을 다룬다.

### 우선 확인할 테스트

- `priority-donate-one`
- `priority-donate-multiple`
- `priority-donate-multiple2`
- `priority-donate-nest`
- `priority-donate-chain`
- `priority-donate-sema`
- `priority-donate-lower`

디버깅 순서는 단일 donation, multiple donation, nested donation, lower priority 순으로 가는 것이 좋다.

## 8. Stage 5: MLFQS

### 목표

`-mlfqs` 옵션이 켜졌을 때 4.4BSD 스타일 스케줄러를 구현한다. 이 모드에서는 priority donation을 사용하지 않는다. priority는 nice, recent_cpu, load_avg 공식으로 계산한다.

MLFQS 모드에서는 thread가 자신의 priority를 직접 제어하지 않는다. 따라서 `thread_create()`로 전달되는 priority 인자는 무시하고 scheduler 공식으로 초기 priority를 계산해야 한다. `thread_set_priority()`도 직접 priority를 바꾸지 않아야 하며, `thread_get_priority()`는 scheduler가 계산한 현재 priority를 반환해야 한다.

initial thread의 nice 값은 0, recent_cpu 값은 0에서 시작한다. 이후 새로 생성되는 thread는 부모 thread의 nice와 recent_cpu를 상속해야 한다. 이 상속 규칙이 빠지면 nice/recent_cpu 관련 테스트에서 기대 출력과 어긋날 수 있다.

### 필요한 필드

대체로 다음 필드가 필요하다.

- 모든 스레드를 담는 `all_list`
- `all_list`에 넣기 위한 별도 `list_elem`
- 각 스레드의 `nice`
- 각 스레드의 `recent_cpu`
- 전역 `load_avg`

기본 코드에 `all_list`가 없다면, MLFQS에서 모든 스레드를 순회하기 위해 추가해야 한다. `thread_init()`에서 초기화하고, `init_thread()` 또는 `thread_create()` 흐름에서 새 스레드를 all_list에 넣는 식으로 설계한다.

`all_list`는 ready_list와 목적이 다르다. ready_list에는 ready 상태의 thread만 들어가지만, MLFQS의 매초 `recent_cpu` 갱신은 running, ready, blocked thread 전체를 대상으로 한다. 따라서 `struct thread.elem`을 all_list에 재사용하면 안 되고, 예를 들어 `allelem` 같은 별도 list element가 필요하다.

생명주기도 같이 관리해야 한다. initial thread도 `thread_get_recent_cpu()`, `thread_get_priority()`의 대상이 되고 스케줄러 계산에 참여하므로 전체 목록에서 추적해야 한다. idle thread는 구조체를 가지지만 실제 작업이 아니므로 `load_avg`, `recent_cpu`, priority 재계산에서 제외해야 한다. 종료되는 thread가 `destruction_req`를 거쳐 해제되는 구조를 고려해, 이미 해제된 thread의 list element가 all_list에 남지 않게 제거 시점을 정해야 한다.

구현할 때는 `struct thread.elem`을 all_list에 쓰지 말고 `allelem` 같은 별도 필드를 추가한다. thread가 `THREAD_DYING` 상태가 되어 실제 page가 해제되기 전에는 all_list에서도 제거되어야 한다. all_list 삽입, 제거, 전체 순회는 scheduler 관련 공유 자료구조를 다루는 작업이므로 interrupt disabled 구간에서 수행하는 편이 안전하다. 그렇지 않으면 매초 MLFQS 계산에서 이미 해제되었거나 해제 대기 중인 thread를 순회해 list 손상이나 page fault가 날 수 있다.

all_list를 순회하는 도중에 현재 순회 중인 list element를 바로 제거하면 다음 element 포인터가 깨질 수 있다. 종료 처리 경로에서 제거 시점을 별도로 정하거나, 순회 중 제거가 필요하다면 next 포인터를 먼저 저장하는 방식처럼 list 순회 불변식을 지켜야 한다.

### fixed-point helper

커널에서는 floating point를 쓰면 안 된다. 따라서 fixed-point helper를 만든다.

예시 개념은 다음과 같다.

```text
F = 1 << 14
int_to_fp(n) = n * F
fp_to_int_zero(x) = x / F
fp_to_int_nearest(x) = x >= 0이면 (x + F / 2) / F, x < 0이면 (x - F / 2) / F
add_fp(x, y) = x + y
sub_fp(x, y) = x - y
add_mixed(x, n) = x + n * F
mul_fp(x, y) = (int64_t)x * y / F
div_fp(x, y) = (int64_t)x * F / y
```

중간 계산에 `int64_t`를 쓰지 않으면 overflow로 테스트가 틀릴 수 있다.

반올림 규칙은 음수에서 특히 조심해야 한다. C의 정수 나눗셈은 0 방향으로 버림 처리되므로, 양수와 음수에 같은 `+ F / 2` 보정을 적용하면 음수 fixed-point 값을 잘못 반올림할 수 있다.

### 업데이트 주기

`thread_tick()`에서 다음 흐름을 관리하는 것이 일반적이다.

```text
매 tick:
    running thread가 idle이 아니면 recent_cpu += 1

매 TIMER_FREQ tick:
    load_avg 갱신
    모든 thread의 recent_cpu 갱신

매 4 tick:
    모든 thread의 priority 갱신
    ready_list 정렬 재정비

time slice가 끝나면:
    intr_yield_on_return()
```

공식 자체는 KAIST 문서의 Advanced Scheduler 요구사항을 따른다. 구현 시에는 "언제 계산하는가"와 "어떤 thread를 계산 대상에 포함하는가"가 중요하다.

특히 `load_avg`와 전체 thread의 `recent_cpu` 갱신은 `timer_ticks() % TIMER_FREQ == 0`이 되는 시점에 정확히 수행해야 한다. 테스트는 이 갱신 시점을 가정하므로, 1초 근처의 다른 tick에서 갱신하면 계산식이 맞아도 출력이 틀어질 수 있다.

또한 tick이 증가한 뒤 일반 kernel thread가 새 tick 값을 관찰하기 전에 scheduler 관련 값 갱신이 끝나야 한다. 그래서 이 작업은 보통 timer interrupt 경로의 `thread_tick()` 안에서 처리한다. 그래야 어떤 thread가 `timer_ticks()`로 새 시간을 본 뒤에도 이전 `load_avg`나 `recent_cpu` 값을 읽는 불일치를 줄일 수 있다.

### priority 계산

MLFQS priority는 다음 직관을 따른다.

- `recent_cpu`가 높을수록 priority가 낮아진다.
- `nice`가 높을수록 priority가 낮아진다.
- 계산 결과는 `PRI_MIN`과 `PRI_MAX` 범위로 clamp해야 한다.

구현 순서는 다음처럼 잡는 것이 안전하다.

```text
recent_term = recent_cpu / 4를 절단(truncated) 방식으로 정수 변환한다.
priority_int = PRI_MAX - recent_term - nice * 2
priority_int가 PRI_MAX보다 크면 PRI_MAX로 낮춘다.
priority_int가 PRI_MIN보다 작으면 PRI_MIN으로 올린다.
```

KAIST 공식은 priority 계산 결과를 nearest rounding이 아니라 정수 절단 방식으로 사용한다. fixed-point helper로 말하면 `thread_get_recent_cpu()`처럼 100배 값을 반올림해 반환하는 getter용 변환과, priority 계산에 쓰는 정수 절단 변환을 혼동하면 안 된다.

priority를 재계산한 뒤 ready_list가 정렬 기반이라면 반드시 다시 정렬해야 한다. 그렇지 않으면 계산값은 맞는데 실행 순서가 틀리는 상황이 생긴다.

### nice 설정

`thread_set_nice(nice)`는 현재 스레드의 nice를 바꾸고 priority를 즉시 다시 계산해야 한다. 그 결과 자신보다 높은 priority의 ready thread가 있으면 yield해야 한다.

### getter 반환값

`thread_get_load_avg()`와 `thread_get_recent_cpu()`는 내부 fixed-point 값을 그대로 반환하지 않는다. 요구사항에 맞게 100배 값으로 변환해 정수로 반환해야 한다. 반올림 규칙이 틀리면 MLFQS 테스트가 한두 줄 차이로 실패할 수 있다.

### 우선 확인할 테스트

- `mlfqs-load-1`
- `mlfqs-load-60`
- `mlfqs-load-avg`
- `mlfqs-recent-1`
- `mlfqs-fair-2`
- `mlfqs-fair-20`
- `mlfqs-block`
- `mlfqs-nice-2`
- `mlfqs-nice-10`

MLFQS 테스트는 오래 걸릴 수 있으므로, 기본 priority/donation 테스트와 분리해 실행하는 것이 좋다.

## 9. 테스트 실패 시 보는 순서

테스트가 실패하면 먼저 실패한 테스트 이름을 기준으로 범위를 좁힌다.

| 실패 테스트 | 먼저 의심할 부분 |
|---|---|
| `alarm-zero`, `alarm-negative` | `timer_sleep()`에서 ticks <= 0 처리 |
| `alarm-single`, `alarm-multiple` | sleeping list 삽입, wakeup tick 계산 |
| `alarm-simultaneous` | 같은 tick에 깨는 여러 스레드 처리 |
| `alarm-priority` | wakeup 후 ready_list priority 정렬 |
| `priority-preempt` | unblock/create 이후 즉시 선점 |
| `priority-fifo` | 같은 priority에서 순서 보존 |
| `priority-sema` | `sema_up()`이 highest waiter를 깨우는지 |
| `priority-condvar` | `cond_signal()`이 highest waiter를 가진 semaphore_elem을 고르는지 |
| `priority-donate-one` | lock acquire 전 donation |
| `priority-donate-multiple` | 여러 donation 중 최댓값 유지 |
| `priority-donate-nest`, `priority-donate-chain` | `waiting_lock`을 따라 donation 전파 |
| `priority-donate-lower` | base priority와 effective priority 분리 |
| `mlfqs-load-*` | ready 상태 스레드 + 현재 running 스레드 계산, idle thread 제외 |
| `mlfqs-recent-*` | recent_cpu 공식과 갱신 주기 |
| `mlfqs-fair-*` | priority 재계산과 ready_list 재정렬 |
| `mlfqs-nice-*` | nice 반영과 즉시 priority 갱신 |

한 번에 여러 테스트가 실패하면 가장 앞 단계의 테스트부터 잡는다. 예를 들어 `priority-donate-*`가 모두 실패하고 `priority-preempt`도 실패한다면 donation보다 preemption부터 고쳐야 한다.

## 10. 디버깅 팁

### 출력 디버깅은 최소화한다

PintOS 테스트는 출력 비교에 민감하다. 테스트 결과에 영향을 줄 수 있는 `printf()`를 무분별하게 넣으면 구현이 맞아도 실패할 수 있다. 디버깅 출력은 임시로 넣고 반드시 제거한다.

### assertion을 활용한다

커널 코드에서는 잘못된 상태를 조용히 넘기면 나중에 더 어려운 버그가 된다. 다음 조건은 assertion으로 확인할 가치가 있다.

- lock release는 holder만 수행한다.
- `thread_block()` 호출 시 interrupt가 꺼져 있다.
- list에 넣기 전 스레드 상태가 의도한 상태다.
- priority 값은 `PRI_MIN`과 `PRI_MAX` 범위 안에 있다.
- donation 전파 중 lock과 holder 포인터가 유효하다.

### 리스트 원소 중복 삽입을 의심한다

PintOS list는 하나의 `list_elem`이 동시에 두 리스트에 들어갈 수 없다. `struct thread.elem`은 ready_list 또는 semaphore waiters 등에서 쓰인다. sleeping list처럼 ready/semaphore waiters와 상태상 겹치지 않는 목록은 같은 elem을 재사용할 수도 있지만, all_list나 held locks/donation 추적처럼 ready/block 상태와 생명주기가 겹치는 목록에는 별도 elem이 필요하다.

필요하면 별도의 `list_elem` 필드를 추가한다. 예를 들어 thread를 all_list에도 넣고 ready_list에도 넣어야 한다면 `allelem`과 `elem`처럼 분리해야 한다.

### interrupt level 복구를 확인한다

`intr_disable()`을 호출했다면 함수가 끝날 때 반드시 원래 level로 복구해야 한다. 중간 return이 있는 함수에서는 복구 누락이 자주 발생한다.

### MLFQS는 출력 오차를 줄인다

MLFQS 실패는 대부분 다음 중 하나다.

- fixed-point 반올림 오류
- `int64_t` 미사용으로 인한 overflow
- update 주기 오류
- idle thread를 계산 대상에 넣은 오류
- ready thread 수 계산 오류
- priority 갱신 후 ready_list 미정렬
- `recent_cpu`가 음수가 될 수 있는데 0으로 잘못 보정한 오류
- timer interrupt handler에서 너무 많은 일을 해서 테스트 thread가 제때 sleep하지 못하는 오류

공식을 다시 보기 전에 이 항목부터 확인한다.

## 11. 구현 전 체크리스트

코드를 작성하기 전에 팀에서 다음 결정을 맞춰 둔다.

- ready_list는 정렬 삽입 방식으로 할지, pop 시 max 선택 방식으로 할지 결정했다.
- sleeping list는 wakeup tick 기준 정렬 방식으로 할지, 매 tick 순회 방식으로 할지 결정했다.
- base priority와 effective priority 필드 이름을 정했다.
- lock과 thread 중 어디에 donation 정보를 저장할지 결정했다.
- MLFQS에서 사용할 fixed-point format을 정했다.
- all_list를 어떤 elem으로 관리할지 정했다.
- interrupt context에서 yield가 필요한 경우 처리 방식을 정했다.
- 테스트는 alarm, priority, donation, mlfqs 순서로 쪼개서 실행하기로 했다.

설계 결정을 문서화하지 않으면 팀원이 각자 다른 방식으로 코드를 고치다가 충돌하기 쉽다.

## 12. PR 리뷰 체크리스트

Project 1 PR을 리뷰할 때는 구현 스타일보다 불변식 유지 여부를 먼저 본다.

- busy waiting이 완전히 제거되었는가?
- interrupt disabled 구간이 과도하게 길지 않은가?
- 모든 ready_list 삽입/제거 경로에서 priority 규칙이 유지되는가?
- `sema_up()`과 `cond_signal()`이 highest priority waiter를 깨우는가?
- donation이 lock acquire 전에 발생하는가?
- lock release 후 해당 lock 관련 donation만 제거되는가?
- base priority와 effective priority가 분리되어 있는가?
- MLFQS와 donation 로직이 섞이지 않는가?
- fixed-point 계산에서 `int64_t`를 사용하는가?
- 테스트 출력에 영향을 주는 debug print가 남아 있지 않은가?

리뷰는 "이 테스트가 통과했는가"뿐 아니라 "왜 통과하는 설계인가"를 확인해야 한다. PintOS는 한 테스트를 맞추기 위한 임시 조건문이 다른 테스트를 쉽게 깨뜨린다.

## 13. 권장 작업 로그 형식

팀 협업에서는 테스트 결과와 구현 범위를 짧게 남기는 것이 중요하다.

```text
[Project1][Alarm] timer_sleep busy waiting 제거
- 변경: timer.c sleeping list 추가, thread.h wakeup_tick 추가
- 통과: alarm-single, alarm-multiple, alarm-zero, alarm-negative
- 미통과: alarm-priority
- 다음 작업: ready_list priority 정렬 후 alarm-priority 재확인
```

이 정도만 남겨도 다음 사람이 이어받기 쉽다. 특히 실패 테스트를 숨기지 않는 것이 중요하다.

## 14. 참고 문서

- KAIST PintOS Project 1 Introduction: https://casys-kaist.github.io/pintos-kaist/project1/introduction.html
- KAIST PintOS Alarm Clock: https://casys-kaist.github.io/pintos-kaist/project1/alarm_clock.html
- KAIST PintOS Priority Scheduling: https://casys-kaist.github.io/pintos-kaist/project1/priority_scheduling.html
- KAIST PintOS Advanced Scheduler: https://casys-kaist.github.io/pintos-kaist/project1/advanced_scheduler.html
- KAIST PintOS Project 1 FAQ: https://casys-kaist.github.io/pintos-kaist/project1/FAQ.html
