# Priority Scheduling: 바뀌어야 할 동작 정리

KAIST Pintos 문서의 Priority Scheduling 요구사항과 테스트 파일(`pintos/tests/threads/priority-*`, `alarm-priority`)을 기준으로,
현재 Pintos에서 **바뀌어야 할 동작**을 정리한다.

> 이 문서는 "무엇이 바뀌어야 하는가"만 다룬다. 구현 위치/방법은 각자 찾는다.

## 문제 정의

1. 현재 스케줄러는 우선순위를 전혀 고려하지 않는다 (라운드로빈, 시간 기반 선점만 존재)
2. 우선순위 스케줄링을 도입하면 우선순위 역전(priority inversion)이 생긴다 → donation으로 완화한다

## 진행 순서 및 난이도

**A → B → C** (난이도: **B > C > A**)

| 이슈 | 난이도 | 근거 |
|---|---|---|
| **A. 우선순위 스케줄러** | ★★☆☆☆ | "순서를 우선순위로" + "더 높으면 양보" 두 원칙으로 정리됨. 테스트 배점 1~2점 |
| **B. Priority Donation** | ★★★★★ | 스레드가 기억해야 할 정보가 새로 생기고, 여러 기부·연쇄 기부 등 경우의 수가 많음. 배점 3점짜리 테스트(`multiple`, `multiple2`, `nest`, `chain`)가 모두 여기에 있음 |
| **C. 맞물리는 지점** | ★★★★☆ | 새로 만드는 양은 적지만, A와 B가 섞인 상황이라 놓치기 쉽고 실패 원인 구분이 가장 어려움 |

---

## A. 우선순위 스케줄러

### 개요

스레드가 실행되거나 깨어나는 모든 순간에 **가장 높은 우선순위 스레드가 먼저** 되도록 바꾼다.

### 바뀌어야 할 동작

| # | 지금 동작 | 바뀌어야 할 동작 | 관련 테스트 |
|---|---|---|---|
| A-1 | 다음 실행 스레드를 들어온 순서로 고른다 | 가장 높은 우선순위를 고르고, 같으면 들어온 순서로 고른다 | `priority-fifo`, `alarm-priority` |
| A-2 | 높은 스레드가 ready가 돼도 실행 중인 스레드가 계속 돈다 | 즉시 양보한다 | `priority-preempt` |
| A-3 | 자기 우선순위를 낮춰도 계속 돈다 | 더 높은 스레드가 있으면 즉시 양보한다 | `priority-change` |
| A-4 | 기다리던 스레드를 기다린 순서로 깨운다 | 가장 높은 우선순위부터 깨운다 (lock / semaphore / condition variable) | `priority-sema`, `priority-condvar` |

### 제약 조건

- 우선순위 범위: `PRI_MIN`(0) ~ `PRI_MAX`(63), 기본값 `PRI_DEFAULT`(31). `PRI_` 값은 변경 금지
- 스레드는 **자기 우선순위만** 변경 가능
- 우선순위 스케줄링은 lock / semaphore / condition variable **모든 경우**에 적용

### 선행 / 후행

- 선행: 없음 (`alarm-priority`만 Alarm Clock 구현 필요)
- 후행: B, C의 기반

### 난이도: ★★☆☆☆ (하~중)

- A-1 ~ A-3: 하
- A-4: 중 (condition variable의 대기 구조 이해 필요)

### 완료 기준

- [ ] `priority-fifo`
- [ ] `priority-preempt`
- [ ] `priority-change`
- [ ] `priority-sema`
- [ ] `priority-condvar`
- [ ] `alarm-priority` (Alarm Clock 구현 선행 필요)

---

## B. Priority Donation

### 개요

우선순위 스케줄링을 도입하면 우선순위 역전(priority inversion)이 생긴다.

> H가 L이 가진 lock을 기다리는 동안 M이 ready 상태면, L이 CPU를 받지 못해 H도 계속 기다리게 된다.

이를 완화하기 위해 lock을 기다리는 스레드가 lock 주인에게 우선순위를 **기부(donation)** 하도록 바꾼다.

### 바뀌어야 할 동작

| # | 지금 동작 | 바뀌어야 할 동작 | 관련 테스트 |
|---|---|---|---|
| B-1 | lock을 기다려도 아무 일이 없다 | lock 주인에게 우선순위를 기부하고, lock을 놓으면 회수한다 | `priority-donate-one` |
| B-2 | 기부가 없다 | 여러 기부를 동시에 기억하고, 일부만 회수할 수 있다 | `priority-donate-multiple`, `priority-donate-multiple2` |
| B-3 | 기부가 없다 | 기부가 lock 대기를 따라 이어진다 (H → M → L) | `priority-donate-nest`, `priority-donate-chain` |
| B-4 | 우선순위 변경이 바로 적용된다 | 기부받는 중에 낮춘 값은 기부 회수 뒤에 적용된다 | `priority-donate-lower` |
| B-5 | 자기 우선순위만 조회된다 | 기부받은 값이 있으면 더 높은 값이 조회된다 | `priority-donate-*` 전반 |

### 제약 조건

- donation은 **lock에만** 적용 (semaphore, condition variable은 제외)
- nested donation 깊이는 8단계 정도로 제한 가능
- 다른 스레드의 우선순위를 직접 바꾸는 인터페이스는 만들지 않음

### 선행 / 후행

- 선행: **A 완료 필요**
- 후행: C

### 난이도: ★★★★★ (상)

- B-1, B-5: 중
- B-4: 중상 ("원래 우선순위"와 "실제 우선순위"의 구분 필요)
- B-2: 상 (여러 기부 중 일부만 회수했을 때 남은 기부 반영)
- B-3: 상 (기부가 여러 단계로 전파)

### 완료 기준

- [ ] `priority-donate-one`
- [ ] `priority-donate-multiple`
- [ ] `priority-donate-multiple2`
- [ ] `priority-donate-nest`
- [ ] `priority-donate-chain`
- [ ] `priority-donate-lower`

---

## C. 스케줄러와 Donation이 맞물리는 지점

### 개요

A(스케줄러)와 B(donation)가 각각 맞게 동작해도, **둘이 동시에 일어날 때** 순서가 깨질 수 있다.
우선순위가 "나중에" 바뀌는 상황에서도 A의 규칙이 유지되도록 한다.

### 바뀌어야 할 동작

| # | 바뀌어야 할 동작 | 관련 테스트 |
|---|---|---|
| C-1 | 기부 회수로 우선순위가 떨어졌을 때도, 더 높은 스레드가 있으면 즉시 양보한다 | `priority-donate-*` 전반 |
| C-2 | 줄(ready list, 대기열)에 들어간 뒤 우선순위가 바뀐 스레드도, 꺼낼 때는 바뀐 우선순위 기준으로 순서가 정해진다 | `priority-donate-nest`, `priority-donate-chain` |
| C-3 | lock 기부와 semaphore 대기가 섞여도 A-4와 B-1이 함께 올바르게 동작한다 | `priority-donate-sema` |

### 선행 / 후행

- 선행: **A, B 완료 필요**
- 후행: 없음

### 난이도: ★★★★☆ (중상)

- 구현할 양은 적지만, 문제가 생겼을 때 원인 찾기가 가장 어렵다
- 테스트 실패가 A, B, C 중 어디 문제인지 구분하기 어려움

### 완료 기준

- [ ] `priority-donate-sema`
- [ ] C-1, C-2가 반영된 상태에서 A, B의 테스트 전체 재통과
  - [ ] `priority-fifo` / `priority-preempt` / `priority-change` / `priority-sema` / `priority-condvar` / `alarm-priority`
  - [ ] `priority-donate-one` / `multiple` / `multiple2` / `nest` / `chain` / `lower`

---

## 참고

- KAIST Pintos 문서: Project 1 Threads > Priority Scheduling
- 테스트 배점: `pintos/tests/threads/Rubric.priority`
