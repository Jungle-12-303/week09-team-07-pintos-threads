# 9주차 PintOS Threads CS 개념 학습 노트

이 문서는 PintOS `Project 1: Threads` 구현 전에 이해해야 할 운영체제와 컴퓨터과학의 기본 원리를 정리한 문서다. 목적은 특정 파일을 어떻게 고칠지 알려주는 것이 아니라, 왜 그런 구현이 필요한지 설명하는 것이다.

구현 순서, 수정 파일, 테스트별 디버깅 방법은 `week9_pintos_project1_implementation_guide.md`에서 다룬다. 이 문서는 그 전에 읽는 배경지식 문서로 보면 된다.

## 1. 운영체제가 스레드를 다루는 이유

논리 CPU 하나는 한 순간에 하나의 실행 흐름만 실행할 수 있다. 그런데 사용자는 여러 프로그램이 동시에 실행되는 것처럼 느낀다. 웹 브라우저가 열려 있고, 음악이 재생되고, 터미널에서 빌드가 돌고, 마우스 입력도 동시에 처리된다.

단일 논리 CPU에서는 운영체제가 CPU 시간을 아주 짧은 단위로 쪼개 여러 실행 흐름에 번갈아 배분하기 때문에 동시에 실행되는 것처럼 보인다. 여러 CPU 코어가 있다면 여러 실행 흐름이 실제로 같은 시각에 실행될 수도 있다. 전자를 동시성, 후자를 병렬성이라고 구분할 수 있다.

| 개념 | 의미 | 예시 |
|---|---|---|
| 동시성 | 여러 작업을 번갈아 진행해 동시에 처리되는 것처럼 보이게 한다 | 단일 CPU에서 여러 스레드를 시분할 실행 |
| 병렬성 | 여러 작업이 실제로 같은 시각에 실행된다 | 여러 CPU 코어에서 여러 스레드를 동시에 실행 |

운영체제는 다음 질문을 계속 판단한다.

- 지금 실행 중인 흐름을 계속 실행할 것인가?
- 다른 흐름으로 CPU를 넘길 것인가?
- 어떤 흐름을 다음에 실행할 것인가?
- 기다릴 일이 있는 흐름을 CPU 경쟁에서 제외할 것인가?

PintOS Threads 프로젝트는 이 질문에 대한 운영체제의 답을 직접 구현해 보는 과제다. Alarm Clock은 "기다리는 스레드를 어떻게 재울 것인가"를 묻고, Priority Scheduling은 "누가 먼저 CPU를 가져야 하는가"를 묻고, Priority Donation은 "우선순위가 꼬였을 때 어떻게 복구할 것인가"를 묻고, MLFQS는 "CPU 사용 패턴을 보고 우선순위를 동적으로 조정할 수 있는가"를 묻는다.

## 2. 프로그램, 프로세스, 스레드

초심자가 가장 먼저 구분해야 할 개념은 프로그램, 프로세스, 스레드다.

프로그램은 디스크에 저장된 실행 가능한 코드와 데이터다. 아직 실행 중인 존재가 아니라 파일에 가깝다.

프로세스는 실행 중인 프로그램이다. 프로세스는 보통 자신만의 주소 공간, 열린 파일 목록, 메모리 매핑, 실행 상태를 가진다. 운영체제는 프로세스를 자원 소유 단위로 본다.

일반적인 사용자 프로그램에서 스레드는 프로세스 안에서 CPU가 실제로 실행하는 흐름이다. 같은 프로세스 안의 여러 스레드는 코드와 전역 데이터, heap을 공유할 수 있지만, 각자 stack과 register 상태를 가진다. 운영체제는 스레드를 CPU 스케줄링 단위로 본다.

다만 커널 스레드는 특정 사용자 프로세스에 속한 실행 흐름이 아니라, 커널이 직접 관리하는 실행 흐름일 수 있다. PintOS Project 1에서 먼저 다루는 것은 이 커널 내부의 실행 흐름이다.

정리하면 다음과 같다.

| 개념 | 핵심 의미 | 운영체제 관점 |
|---|---|---|
| 프로그램 | 실행 가능한 파일 | 아직 실행 전 |
| 프로세스 | 실행 중인 프로그램 인스턴스 | 자원 소유 단위 |
| 스레드 | CPU가 따라가는 실행 흐름 | 스케줄링 단위 |

PintOS Project 1에서는 사용자 프로그램보다 먼저 커널 스레드를 다룬다. 즉, 아직 파일 실행이나 system call을 본격적으로 다루기 전에, 운영체제 내부 실행 흐름을 어떻게 만들고 멈추고 다시 실행할지를 학습한다.

## 3. 커널 스레드와 사용자 스레드

스레드는 크게 사용자 수준 스레드와 커널 수준 스레드로 나눌 수 있다.

사용자 수준 스레드는 사용자 라이브러리가 관리한다. 생성과 전환이 빠를 수 있지만, many-to-one 모델처럼 커널이 각 사용자 스레드를 직접 알지 못하는 구조에서는 하나의 스레드가 blocking system call에 들어갔을 때 같은 프로세스의 다른 스레드까지 영향을 받을 수 있다.

커널 수준 스레드는 운영체제 커널이 직접 알고 관리한다. 커널은 각 스레드의 상태를 추적하고, timer interrupt나 I/O 완료 같은 사건에 따라 어떤 스레드를 실행할지 결정한다. PintOS에서 다루는 스레드는 이 커널 수준 스레드 모델에 가깝다.

커널 스레드를 이해할 때 중요한 점은 스레드가 단순한 함수 호출이 아니라는 것이다. 스레드는 자신의 stack, register 상태, 현재 실행 위치를 가진 독립적인 실행 흐름이다. 따라서 스레드를 멈췄다가 나중에 다시 실행하려면 "어디까지 실행했는지"를 저장해 두어야 한다.

## 4. Thread Control Block

운영체제는 각 스레드를 관리하기 위해 제어 정보를 저장한다. 이를 보통 TCB, 즉 Thread Control Block이라고 부른다.

TCB에는 다음과 같은 정보가 들어간다.

- 스레드 식별자
- 현재 상태
- 우선순위
- stack 위치
- 저장된 register 값
- 스케줄러가 사용할 연결 정보
- 동기화나 sleep 상태에 필요한 부가 정보

중요한 점은 TCB가 "스레드 자체"가 아니라 스레드를 관리하기 위한 운영체제의 장부라는 것이다. CPU는 register와 stack을 보고 코드를 실행하지만, 운영체제는 TCB를 보고 이 스레드가 running인지, ready인지, blocked인지 판단한다.

PintOS에서는 thread 구조체와 kernel stack이 밀접하게 붙어 있다. 이 구조는 교육용 OS에서 이해하기 쉽지만, stack overflow나 구조체 비대화에 취약하다. 따라서 커널 코드에서는 큰 지역 배열이나 깊은 재귀를 피해야 한다. 이것은 PintOS만의 습관이 아니라 커널 프로그래밍 일반에서 중요한 감각이다.

## 5. Context Switch

Context switch는 CPU가 실행하던 스레드를 멈추고 다른 스레드로 전환하는 과정이다. 여기서 context란 "나중에 이어서 실행하기 위해 필요한 CPU 상태"를 의미한다.

대표적인 context 정보는 다음과 같다.

- instruction pointer: 다음에 실행할 명령 위치
- stack pointer: 현재 stack 위치
- general-purpose registers: 계산 중이던 값들
- flags: CPU 상태 플래그
- 주소 공간 정보: 프로세스가 바뀌는 경우 필요

context switch는 공짜가 아니다. 저장과 복원이 필요하고, CPU cache나 TLB 효율에도 영향을 줄 수 있다. 그래서 운영체제는 너무 자주 전환하면 부가 비용이 커지고, 너무 드물게 전환하면 반응성이 떨어지는 균형 문제를 해결해야 한다.

Project 1의 time slice 개념은 이 균형과 관련이 있다. 한 스레드가 CPU를 너무 오래 독점하지 못하게 일정 시간이 지나면 운영체제가 개입한다.

## 6. 스레드 상태: Running, Ready, Blocked

스레드는 계속 실행 중이기만 한 것이 아니다. 운영체제는 스레드를 상태로 구분한다.

| 상태 | 의미 |
|---|---|
| Running | 지금 CPU에서 실행 중 |
| Ready | 실행 가능하지만 CPU를 기다림 |
| Blocked | 어떤 사건이 일어나기를 기다림 |
| Dying/Terminated | 종료 중이거나 종료됨 |

Ready와 Blocked의 차이가 중요하다. Ready는 CPU만 받으면 바로 실행할 수 있는 상태다. Blocked는 CPU를 받아도 진행할 수 없는 상태다. 예를 들어 lock을 기다리거나, timer가 만료되기를 기다리거나, I/O 완료를 기다리는 스레드는 blocked 상태가 되어야 한다.

이 구분을 잘못하면 CPU 낭비가 생긴다. 실제로는 기다릴 수밖에 없는 스레드가 ready 상태에 남아 있으면, 운영체제는 그 스레드도 실행 후보로 계속 고려한다. 스레드는 실행되자마자 "아직 기다릴 시간이 안 됐다"를 확인하고 다시 양보한다. 이것이 busy waiting이다.

## 7. 선점형 스케줄링과 협력형 스케줄링

스케줄링 방식은 크게 협력형과 선점형으로 나눌 수 있다.

협력형 스케줄링에서는 실행 중인 스레드가 스스로 CPU를 양보해야 다른 스레드가 실행된다. 구현은 단순하지만, 한 스레드가 양보하지 않으면 전체 시스템 반응성이 나빠질 수 있다.

선점형 스케줄링에서는 운영체제가 timer interrupt 등을 이용해 실행 중인 스레드를 강제로 멈추고 다른 스레드로 전환할 수 있다. 현대 운영체제는 보통 선점형 스케줄링을 사용한다.

선점형 스케줄링의 핵심은 interrupt다. CPU가 사용자 코드나 커널 코드를 실행하는 중에도 timer interrupt가 들어오면, CPU는 하던 일을 잠시 멈추고 interrupt handler로 제어를 넘긴다. 운영체제는 이 시점에 "현재 스레드가 time slice를 다 썼는가", "더 높은 우선순위 스레드가 있는가" 같은 판단을 한다.

선점은 공정성과 반응성을 높이지만, 동시에 race condition 가능성을 키운다. 실행 흐름이 예측하지 못한 지점에서 끊길 수 있기 때문이다.

PintOS Project 1을 이해할 때는 thread context와 interrupt context를 구분해야 한다.

| 구분 | 의미 | 가능한 일 |
|---|---|---|
| Thread context | 일반 스레드가 자기 stack 위에서 실행 중인 상태 | 문맥상 허용되면 block되거나 yield할 수 있다 |
| Interrupt context | interrupt handler가 실행 중인 상태 | 오래 걸리거나 sleep하는 작업을 하면 안 된다 |

Interrupt context에서는 현재 실행 중인 스레드를 직접 재우거나, sleep할 수 있는 lock acquire를 호출하면 안 된다. 더 높은 priority 스레드가 깨어나 선점이 필요하다면 interrupt handler 안에서 바로 context switch를 하는 대신, interrupt가 끝난 뒤 yield하도록 예약하는 방식이 필요하다.

Thread context라고 해서 언제나 block이나 yield가 안전한 것은 아니다. interrupt를 꺼 둔 임계구역, scheduler 내부, list 구조를 갱신하는 도중처럼 원자성이 필요한 구간에서는 먼저 상태를 일관되게 만든 뒤 CPU를 양보해야 한다.

## 8. Race Condition과 Critical Section

Race condition은 여러 실행 흐름이 같은 데이터를 동시에 건드릴 때, 실행 순서에 따라 결과가 달라지는 문제다.

예를 들어 두 스레드가 같은 counter를 증가시킨다고 하자. `counter++`는 한 줄처럼 보이지만 실제로는 보통 읽기, 증가, 쓰기의 여러 단계로 이루어진다.

```text
counter 값을 읽는다.
읽은 값에 1을 더한다.
결과를 다시 counter에 쓴다.
```

두 스레드가 이 과정을 동시에 수행하면 증가가 한 번 사라질 수 있다. 이런 문제가 생기는 공유 데이터에 접근하는 코드 구간을 critical section이라고 부른다. 공유 데이터 자체가 아니라, 공유 데이터를 읽고 수정하고 다시 쓰는 일련의 실행 구간이 보호 대상이다.

운영체제는 critical section을 보호하기 위해 여러 도구를 사용한다.

- interrupt disable: 현재 CPU에서 interrupt에 의한 선점을 잠시 막는다.
- lock/mutex: 한 번에 하나의 스레드만 critical section에 들어오게 한다.
- semaphore: 제한된 수의 자원 접근을 제어한다.
- atomic instruction: CPU가 쪼갤 수 없는 단일 연산을 제공한다.

PintOS Project 1에서는 짧은 커널 내부 자료구조 조작에 interrupt disable을 자주 사용한다. 하지만 interrupt를 오래 꺼두면 timer interrupt나 I/O 처리가 지연되므로, critical section은 짧고 명확해야 한다.

여기서 "현재 CPU"라는 표현이 중요하다. PintOS Project 1은 사실상 단일 CPU 환경을 전제로 하므로 interrupt를 끄면 현재 실행 흐름의 선점을 막아 커널 자료구조를 안전하게 갱신할 수 있다. 하지만 실제 멀티코어 운영체제에서는 다른 CPU가 같은 자료구조에 접근할 수 있으므로 interrupt disable만으로 충분하지 않고 spinlock 같은 추가 동기화가 필요하다.

## 9. Busy Waiting과 Blocking

Busy waiting은 조건이 만족될 때까지 CPU를 사용하며 반복 확인하는 방식이다.

```text
while 조건이 아직 false:
    계속 확인한다.
```

Blocking은 조건이 만족될 때까지 현재 실행 흐름을 스케줄링 대상에서 제외하는 방식이다.

```text
조건이 아직 false라면:
    나를 blocked 상태로 바꾼다.
    다른 스레드에게 CPU를 넘긴다.
조건을 만족시키는 사건이 발생하면:
    나를 다시 ready 상태로 만든다.
```

짧은 시간의 하드웨어 지연처럼 매우 짧고 정확한 대기가 필요하면 busy waiting이 쓰일 수 있다. 그러나 수 ms, 수 tick 이상 기다리는 상황에서 busy waiting은 CPU 낭비다. 운영체제는 기다릴 수밖에 없는 스레드를 blocked 상태로 보내고, 실제 사건이 발생했을 때 깨우는 방식으로 CPU를 효율적으로 사용한다.

Alarm Clock 과제의 근본 원리는 바로 이 차이다. "시간이 지났는지 계속 확인하는 스레드"를 "시간이 될 때까지 잠든 스레드"로 바꾸는 것이다.

## 10. 동기화 도구 1: Semaphore

Semaphore는 정수 값과 대기열을 가진 동기화 도구다. 정수 값은 사용 가능한 자원의 개수로 이해할 수도 있고, 아직 소비되지 않은 signal 수로 이해할 수도 있다. 그래서 semaphore는 여러 개의 동일한 자원을 제어할 때뿐 아니라, 어떤 사건이 발생했음을 다른 스레드에게 전달할 때도 사용할 수 있다.

Semaphore의 기본 연산은 두 가지다.

- down/P/wait: 자원이 있으면 하나 사용하고 통과한다. 없으면 기다린다.
- up/V/signal: 자원을 하나 반납하고, 기다리는 스레드가 있으면 깨운다.

Semaphore는 다음 상황에 적합하다.

- 동시에 N개까지 접근 가능한 자원을 제어할 때
- producer-consumer 문제에서 항목 개수를 표현할 때
- 특정 사건이 일어날 때까지 스레드를 재울 때

Semaphore는 소유자 개념이 없다. 한 스레드가 down하고 다른 스레드가 up할 수 있다. 이 점이 lock과 다르다.

## 11. 동기화 도구 2: Lock과 Mutex

Lock 또는 mutex는 한 번에 하나의 스레드만 critical section에 들어가도록 하는 도구다.

Lock의 핵심은 소유권이다.

- lock을 획득한 스레드가 owner가 된다.
- owner만 lock을 release할 수 있다.
- 이미 다른 스레드가 lock을 가지고 있으면 기다려야 한다.

Lock은 공유 자료구조를 보호할 때 사용한다. 예를 들어 list에 원소를 넣거나 빼는 중 다른 스레드가 같은 list를 건드리면 포인터 구조가 깨질 수 있다. Lock은 이런 critical section을 하나의 스레드만 실행하게 만든다.

단, lock은 잘못 사용하면 deadlock을 만들 수 있다. 대표적으로 스레드 A가 lock 1을 잡고 lock 2를 기다리고, 스레드 B가 lock 2를 잡고 lock 1을 기다리면 둘 다 영원히 진행하지 못한다. 따라서 lock 획득 순서와 release 위치는 설계 단계에서 명확해야 한다.

Deadlock은 보통 네 조건이 동시에 만족될 때 발생할 수 있다.

| 조건 | 의미 |
|---|---|
| Mutual exclusion | 자원을 한 번에 하나의 스레드만 사용할 수 있다 |
| Hold and wait | 이미 자원을 가진 스레드가 다른 자원을 추가로 기다린다 |
| No preemption | 운영체제가 스레드가 가진 자원을 강제로 빼앗을 수 없다 |
| Circular wait | 스레드들이 원형으로 서로의 자원을 기다린다 |

이 네 조건 중 하나를 깨면 deadlock을 예방할 수 있다. 예를 들어 lock 획득 순서를 전역적으로 정하면 circular wait를 줄일 수 있다.

## 12. 동기화 도구 3: Condition Variable

Condition variable은 어떤 조건이 만족될 때까지 기다리기 위한 도구다. lock이 "critical section에 한 명만 들어오게 하는 도구"라면, condition variable은 "조건이 아직 아니면 잠들었다가 조건이 바뀌면 다시 확인하는 도구"다.

Condition variable은 보통 lock과 함께 사용한다. 이유는 조건 검사와 대기 상태 전환이 원자적으로 이어져야 하기 때문이다.

이 원자성이 깨지면 lost wakeup이 생길 수 있다. 예를 들어 스레드 A가 "조건이 아직 false다"라고 확인한 직후, 아직 condition waiters에 등록되기 전에 스레드 B가 조건을 true로 만들고 signal을 보내면, A는 그 signal을 놓친 채 잠들 수 있다. 그래서 condition variable은 보통 lock을 잡은 상태에서 조건을 검사하고, wait 연산이 lock release와 sleep 등록을 하나의 규칙 안에서 처리하도록 설계한다.

전형적인 사용 패턴은 다음과 같다.

```text
lock을 잡는다.
while 조건이 아직 만족되지 않았다:
    condition variable에서 기다린다.
조건이 만족되었으므로 작업한다.
lock을 푼다.
```

여기서 `if`가 아니라 `while`을 쓰는 이유가 중요하다. 많은 시스템의 condition variable은 Mesa-style semantics를 따른다. signal을 받았다고 해서 조건이 지금도 반드시 참이라는 보장이 없다. signal을 받은 뒤 다시 lock을 얻기 전까지 다른 스레드가 상태를 바꿀 수 있기 때문이다. 따라서 깨어난 뒤 조건을 다시 검사해야 한다.

## 13. 스케줄링 정책: FIFO, Round-Robin, Priority

스케줄러는 ready 상태의 스레드 중 하나를 골라 CPU를 준다. 어떤 기준으로 고르느냐가 스케줄링 정책이다.

FIFO는 먼저 온 스레드를 먼저 실행한다. 단순하지만 긴 작업이 앞에 있으면 짧은 작업들이 오래 기다릴 수 있다.

Round-Robin은 각 스레드에 일정한 time slice를 주고 번갈아 실행한다. 공정성이 좋고 구현도 비교적 단순하지만, 중요한 작업과 덜 중요한 작업을 구분하지 못한다.

Priority Scheduling은 각 스레드에 우선순위를 부여하고, 높은 우선순위 스레드를 먼저 실행한다. 반응성이 중요한 작업을 빨리 처리할 수 있지만, 낮은 우선순위 스레드가 오래 실행되지 못하는 starvation 문제가 생길 수 있다.

좋은 스케줄러는 다음 목표 사이에서 균형을 잡아야 한다.

- 반응성: 중요한 작업을 빨리 처리한다.
- 공정성: 특정 스레드가 영원히 밀리지 않게 한다.
- 처리량: 전체 작업량을 많이 처리한다.
- 예측 가능성: 실행 순서가 너무 불안정하지 않게 한다.
- 부가 비용: 스케줄링 판단 자체에 CPU를 너무 많이 쓰지 않는다.

## 14. Priority Inversion

Priority Scheduling에는 priority inversion이라는 문제가 있다.

상황을 세 스레드 H, M, L로 생각해 보자.

- H는 높은 priority다.
- M은 중간 priority다.
- L은 낮은 priority다.
- L이 lock을 가지고 있다.
- H가 그 lock을 기다린다.
- M은 lock이 필요 없고 ready 상태다.

이때 H는 L이 lock을 풀어야 진행할 수 있다. 그런데 L은 priority가 낮아서 M에게 밀린다. 결과적으로 높은 priority인 H가 중간 priority인 M 때문에 간접적으로 막히는 이상한 상황이 된다.

이것이 priority inversion이다. 우선순위 정책이 오히려 우선순위의 의미를 깨뜨리는 상황이다.

## 15. Priority Donation

Priority Donation은 priority inversion을 줄이기 위한 방법이다. 높은 priority 스레드 H가 낮은 priority 스레드 L이 가진 lock을 기다린다면, H의 priority를 L에게 임시로 빌려준다. 그러면 L은 중간 priority 스레드 M보다 먼저 실행되어 lock을 release할 수 있다.

PintOS Project 1에서 priority donation은 lock에 대해서만 요구된다. Semaphore와 condition variable에서도 높은 priority waiter를 먼저 깨워야 하지만, 그것은 priority-aware wakeup이지 새로운 donation 관계를 만드는 것은 아니다.

또한 MLFQS 모드에서는 priority donation을 사용하지 않는다. MLFQS가 켜져 있으면 priority는 nice, recent_cpu, load_avg 공식으로 동적으로 계산되며, lock donation 로직과 섞이면 안 된다.

Donation에서 중요한 개념은 base priority와 effective priority의 구분이다.

- base priority: 스레드가 원래 가진 priority
- effective priority: donation을 반영해 실제 스케줄링에 쓰는 priority

Donation은 영구 변경이 아니다. lock을 release하면 해당 lock 때문에 생긴 donation은 사라져야 한다. 여러 lock을 가진 스레드가 여러 donation을 받을 수 있으므로, effective priority는 보통 base priority와 donation priority들의 최댓값으로 이해하면 된다.

Donation은 chain으로 전파될 수도 있다. H가 M이 가진 lock을 기다리고, M이 다시 L이 가진 lock을 기다린다면, H의 priority는 M뿐 아니라 L에게도 영향을 주어야 한다. 이것이 nested donation 또는 chained donation이다.

이 개념은 PintOS뿐 아니라 실제 운영체제와 실시간 시스템에서도 중요하다. priority inversion은 이론 문제가 아니라 실제 시스템 장애의 원인이 될 수 있다.

## 16. MLFQS: 동적 우선순위 스케줄링

MLFQS는 Multi-Level Feedback Queue Scheduler의 약자다. 이름을 풀어 보면 다음 세 부분으로 나뉜다.

- Multi-Level: 여러 우선순위 수준을 둔다.
- Feedback: 스레드의 과거 행동을 보고 priority를 조정한다.
- Queue: priority별 ready queue를 사용한다는 아이디어에서 출발한다.

정적 priority scheduling은 사용자가 준 priority를 그대로 따른다. 하지만 모든 작업의 중요도를 사람이 정확히 정하기는 어렵다. CPU를 많이 쓰는 계산 작업과, 잠깐 실행하고 다시 I/O를 기다리는 반응성 작업은 다른 방식으로 다뤄야 한다.

MLFQS의 직관은 다음과 같다.

- 최근에 CPU를 많이 쓴 스레드는 priority를 낮춘다.
- CPU를 오래 받지 못했거나 I/O 중심으로 동작하는 스레드는 비교적 빨리 반응하게 한다.
- 사용자가 nice 값을 통해 "나는 CPU를 덜 받아도 된다" 또는 "조금 더 적극적으로 받고 싶다"는 힌트를 줄 수 있다.
- 시스템 전체 부하를 load average로 반영한다.

이 방식은 starvation을 줄이고, 상호작용이 많은 작업의 반응성을 높이려는 목적을 가진다. PintOS의 MLFQS는 4.4BSD scheduler에서 단순화된 모델을 사용한다.

## 17. Moving Average, recent_cpu, load_avg

MLFQS를 이해하려면 moving average 개념이 필요하다. Moving average는 과거 값들을 모두 똑같이 보지 않고, 최근 값에 더 큰 비중을 두는 평균이다.

`recent_cpu`는 한 스레드가 최근에 CPU를 얼마나 사용했는지 나타낸다. 이름 그대로 전체 실행 시간 총합이 아니라 최근 사용량에 가까운 값이다. CPU를 계속 쓰면 증가하고, 시간이 지나면 load average와 nice 값에 따라 조정된다.

`recent_cpu`는 이름 때문에 항상 0 이상일 것처럼 보이지만, nice 값이 음수이면 음수가 될 수 있다. 이 값은 단순한 누적 CPU 시간이 아니라 scheduler 공식에 들어가는 가중치이므로, 음수가 되었다고 0으로 강제로 보정하면 안 된다.

`load_avg`는 시스템에 실행할 일이 얼마나 쌓여 있는지 나타내는 평균이다. ready 상태이거나 running 상태인 스레드 수가 많으면 load average가 올라간다. 단, idle thread는 실제 작업이 아니므로 부하 계산에서 제외한다.

PintOS에서 사용하는 대표 공식은 다음과 같은 의미를 가진다.

```text
priority = PRI_MAX - recent_cpu / 4 - nice * 2
```

최근에 CPU를 많이 썼거나 nice 값이 크면 priority가 낮아진다.

계산 결과는 정수 priority로 사용되므로 소수부는 절단(truncated) 처리하고, 최종 값은 `PRI_MIN`과 `PRI_MAX` 범위 안으로 clamp해야 한다.

```text
load_avg = 59/60 * load_avg + 1/60 * ready_threads
```

기존 load average를 대부분 유지하면서 현재 실행 대기 중인 작업 수를 조금 반영한다. PintOS 공식에서 `ready_threads`는 ready 상태 스레드 수에 현재 running 스레드를 더한 값이며, idle thread는 제외한다. 그래서 값이 급격히 튀지 않고 천천히 변한다.

```text
recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
```

최근 CPU 사용량을 시스템 부하와 nice 값에 따라 감쇠 또는 보정한다. 여기서 `(2 * load_avg) / (2 * load_avg + 1)` 부분은 시스템 부하가 높을수록 recent_cpu가 천천히 줄어들게 만드는 계수다.

이 공식들은 외우는 것보다 의미를 이해하는 것이 중요하다. 스케줄러는 "최근 CPU를 많이 쓴 스레드에게 계속 CPU를 주지 않고", "대기하던 스레드가 다시 기회를 얻도록" 값을 조정한다.

## 18. Fixed-Point Arithmetic

MLFQS 공식에는 실수가 등장한다. 그러나 커널에서는 floating point 연산을 피하는 경우가 많다. 이유는 단순히 성능 때문만이 아니다. 부동소수점 레지스터 상태까지 context switch에서 저장하고 복원해야 할 수 있고, 커널 안에서 FPU 상태를 잘못 다루면 사용자 프로그램의 부동소수점 계산 상태를 망가뜨릴 수 있다.

그래서 커널에서는 정수만으로 실수 비슷한 값을 표현하는 fixed-point arithmetic을 사용한다.

핵심 아이디어는 간단하다.

```text
실제 값 1.0을 내부적으로 16384로 저장한다.
실제 값 1.5를 내부적으로 24576으로 저장한다.
필요할 때 다시 16384로 나누어 해석한다.
```

즉, 정수의 일부 비트를 소수부처럼 사용한다. PintOS에서 흔히 쓰는 17.14 fixed-point 형식은 하위 14비트를 소수부로 사용한다.

Fixed-point에서 조심할 점은 곱셈과 나눗셈이다. 두 fixed-point 값을 곱하면 scale이 두 번 적용되므로 다시 나눠야 한다. 또한 중간 계산이 커져 overflow가 날 수 있으므로 더 큰 정수 타입으로 중간값을 계산해야 한다.

Fixed-point는 PintOS만의 특수 지식이 아니라, 임베디드 시스템, 커널, 그래픽스, DSP처럼 부동소수점 사용이 제한되거나 비용이 큰 환경에서 자주 등장하는 기법이다.

## 19. 커널 코드에서의 메모리 감각

사용자 프로그램에서는 stack이 비교적 넉넉하다고 느낄 수 있다. 하지만 커널에서는 각 스레드의 kernel stack이 작고 고정된 경우가 많다. 따라서 커널 함수 안에서 큰 지역 배열을 만들거나 깊은 재귀를 사용하면 stack overflow가 발생할 수 있다.

커널 메모리 버그는 일반 애플리케이션보다 더 위험하다.

- 잘못된 포인터 하나가 커널 전체를 멈출 수 있다.
- list 연결이 깨지면 스케줄러가 잘못된 스레드를 선택할 수 있다.
- 이미 해제된 thread 정보를 참조하면 재현하기 어려운 버그가 생긴다.
- interrupt context에서 안전하지 않은 메모리 접근을 하면 시스템 전체가 불안정해진다.

PintOS를 학습할 때 "왜 이렇게 작은 구조체와 list element를 조심해서 다루는가"를 이해하는 것이 중요하다. 이것은 단순한 C 문법 문제가 아니라 운영체제의 안정성과 직결된다.

## 20. 이 문서를 읽은 뒤 가져야 할 관점

Threads 프로젝트에서 중요한 것은 특정 테스트를 맞추는 요령만이 아니다. 다음 관점을 갖고 구현 문서를 읽어야 한다.

- CPU를 받는 것과 실행 가능한 상태는 다르다.
- 기다릴 수밖에 없는 스레드는 ready 상태가 아니라 blocked 상태여야 한다.
- 선점형 스케줄링에서는 언제든 실행 흐름이 끊길 수 있다.
- 공유 자료구조를 건드리는 코드는 원자성을 확보해야 한다.
- lock은 mutual exclusion을 제공하지만 deadlock과 priority inversion을 만들 수 있다.
- semaphore, lock, condition variable은 비슷해 보여도 목적과 소유권 모델이 다르다.
- priority는 반응성을 높이지만 starvation과 inversion 문제를 만든다.
- donation은 priority inversion을 줄이는 임시 보정이다.
- MLFQS는 사용 패턴을 feedback으로 삼아 priority를 동적으로 계산한다.
- 커널에서는 작은 자료구조, 짧은 critical section, 명확한 상태 전이가 중요하다.

이 관점을 가진 상태에서 구현 지침서를 읽으면, 각 코드 변경이 단순한 테스트 대응이 아니라 운영체제 원리를 코드로 옮기는 과정으로 보인다.

## 21. 참고 문서

- KAIST PintOS Introduction: https://casys-kaist.github.io/pintos-kaist/
- KAIST PintOS Project 1 Introduction: https://casys-kaist.github.io/pintos-kaist/project1/introduction.html
- KAIST PintOS Alarm Clock: https://casys-kaist.github.io/pintos-kaist/project1/alarm_clock.html
- KAIST PintOS Priority Scheduling: https://casys-kaist.github.io/pintos-kaist/project1/priority_scheduling.html
- KAIST PintOS Advanced Scheduler: https://casys-kaist.github.io/pintos-kaist/project1/advanced_scheduler.html
