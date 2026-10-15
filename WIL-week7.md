# [크래프톤 정글] WIL 7주차 — Pintos Threads: Alarm Clock, Priority Scheduling, 그리고 맛집으로 이해한 세마포어

> **WIL (Weekly I Learned)**: 이번 주에 배운 것, 헷갈렸던 것, 다음 주에 할 것을 정리한다.

---

## 📌 이번 주 한 줄 요약

> **"기다리는 스레드는 CPU를 붙잡지 말고 잠들어야 하고, CPU는 가장 급한 스레드에게 가야 한다."**

| 날짜 | 한 일 |
|---|---|
| 10/8 | Pintos 개발 환경(Docker + DevContainer) 세팅, 이번 주 이슈 목록 정리 |
| 10/11 | `thread.c`, `timer.c` 코드 읽기 시작 |
| 10/12 | Priority Scheduling 요구사항을 **A(스케줄러) / B(Donation) / C(맞물리는 지점)** 로 나눠 정리 |
| 10/13 | **Alarm Clock** 구현 (busy waiting → `sleep_list`), 우선순위 기반 `next_thread_to_run` / 선점 구현 |
| 10/13~14 | **세마포어**를 "맛집" 비유로 정리하며 발표 준비, 복습 퀴즈로 헷갈리는 점 정리 |

---

## 1. Alarm Clock — 바쁜 대기(busy waiting)를 없애기

### 원래 코드의 문제

```c
void
timer_sleep (int64_t ticks) {
	int64_t start = timer_ticks ();
	while (timer_elapsed (start) < ticks)
		thread_yield ();
}
```

- "시간 됐나?"를 **계속 확인**하면서 READY ↔ RUNNING을 오간다.
- 잠든 게 아니라 **깨어 있는 채로 계속 줄을 서는 것**이라 CPU를 낭비한다.

### 바꾼 방법: 재울 때 기록하고, 타이머가 깨운다

**`timer_sleep()`** — 가드 → 깨어날 시각 계산 → 인터럽트 끄기 → 정렬 삽입 → 블록 → 인터럽트 복구

```c
void
timer_sleep (int64_t ticks) {
	if (ticks <= 0) return;
	ASSERT (intr_get_level () == INTR_ON);

	struct thread *cur = thread_current ();
	cur->wakeup_tick = timer_ticks () + ticks;   // 넣기 전에 값부터 계산

	enum intr_level old = intr_disable ();

	/* wakeup_tick 오름차순으로 끼워 넣기 → 깨울 때 앞에서부터만 보면 된다 */
	struct list_elem *e = list_begin (&sleep_list);
	while (e != list_end (&sleep_list)) {
		struct thread *t = list_entry (e, struct thread, elem);
		if (cur->wakeup_tick < t->wakeup_tick) break;
		e = list_next (e);
	}
	list_insert (e, &cur->elem);

	thread_block ();
	intr_set_level (old);
}
```

**`timer_interrupt()`** — 매 틱마다 리스트 앞에서부터 깨울 시각이 된 스레드를 꺼낸다

```c
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	struct list_elem *e = list_begin (&sleep_list);
	while (e != list_end (&sleep_list)) {
		struct thread *t = list_entry (e, struct thread, elem);
		if (t->wakeup_tick > ticks) break;   // 정렬돼 있으니 여기서 멈춰도 된다
		e = list_remove (e);                 // remove가 다음 원소를 돌려준다
		thread_unblock (t);
	}
	thread_tick ();
}
```

### 배운 점

- **정렬 삽입의 이유**: 넣을 때 한 번 정렬해 두면, 매 틱마다 실행되는 인터럽트 핸들러는 **앞에서 몇 개만 보고 `break`** 할 수 있다. 자주 실행되는 쪽을 가볍게 만드는 게 핵심이다.
- **인터럽트를 끄는 이유**: `sleep_list`는 `timer_sleep()`과 인터럽트 핸들러가 **같이 건드리는 공유 자료구조**다. 리스트를 고치는 도중에 타이머 인터럽트가 끼어들면 리스트가 깨진다. 인터럽트 핸들러 안에서는 락을 쓸 수 없으니(잠들 수 없으니) 인터럽트를 끄는 방법으로 보호한다.
- **`list_remove()`의 반환값**: 지운 원소의 **다음 원소**를 돌려준다. 이걸 받아야 순회를 이어갈 수 있다. `list_next(e)`를 지운 뒤에 부르면 안 된다.
- **`elem` 재사용**: 잠든 스레드는 `ready_list`에 없으므로, 같은 `elem`을 `sleep_list`에 써도 겹치지 않는다. (한 스레드는 동시에 두 리스트에 있지 않다)

### 🐛 삽질

- `wakeup_tick` 필드를 `struct thread`에 추가했는데 반영이 안 되는 것 같았다.
  → `pintos/threads/thread.h`와 `pintos/include/threads/thread.h` **두 파일이 있었고**, include 경로상 다른 쪽이 먼저 잡히고 있었다. 두 곳 모두 고쳐서 해결.

---

## 2. Priority Scheduling — "누가 먼저 CPU를 쓰나"

### 문제를 먼저 쪼갰다

구현에 들어가기 전에 KAIST 문서와 테스트 파일(`priority-*`)을 보고 **"무엇이 바뀌어야 하는가"** 만 먼저 정리했다.

| 이슈 | 내용 | 난이도 |
|---|---|---|
| **A. 우선순위 스케줄러** | 가장 높은 스레드부터 실행 + 더 높은 스레드가 생기면 즉시 양보 | ★★☆☆☆ |
| **B. Priority Donation** | 락을 기다리는 높은 스레드가 락 주인에게 우선순위를 빌려줌 (여러 기부, 연쇄 기부) | ★★★★★ |
| **C. 맞물리는 지점** | 기부 회수 후 양보, 줄에 들어간 뒤 우선순위가 바뀐 경우 | ★★★★☆ |

진행 순서는 **A → B → C**. 이번 주에는 A의 핵심을 구현했다.

### A에서 구현한 것

**① 다음 스레드는 가장 높은 우선순위로 고른다** (`next_thread_to_run`)

```c
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	struct list_elem *max_elem = list_max (&ready_list, compare_priority, NULL);
	list_remove (max_elem);
	return list_entry (max_elem, struct thread, elem);
}
```

**② 더 높은 스레드가 생기면 바로 양보한다** (`thread_create`, `thread_set_priority`)

```c
/* thread_create: 새 스레드가 더 높으면 양보 (같으면 양보하지 않음 → FIFO 유지) */
if (t->priority > thread_current ()->priority)
	thread_yield ();

/* thread_set_priority: 내 우선순위를 낮췄는데 더 높은 스레드가 있으면 양보 */
if (!list_empty (&ready_list) &&
    list_entry (list_max (&ready_list, compare_priority, NULL),
                struct thread, elem)->priority > new_priority)
	thread_yield ();
```

### 🐛 삽질: `list_remove(list_max(...))` 후 다시 `list_max(...)`

처음엔 이렇게 쓰려고 했다.

```c
list_remove (list_max (&ready_list, compare_priority, NULL));
return list_entry (list_max (&ready_list, compare_priority, NULL), struct thread, elem);
```

- `A(10) B(40) C(20)` 에서 첫 `list_max`가 **B를 지운다.**
- 두 번째 `list_max`는 남은 `A(10) C(20)`에서 다시 찾으니 **C가 반환된다.**
- → 실행돼야 할 B는 리스트에서 사라지고, 엉뚱한 C가 실행된다.

**함수 호출 결과를 변수에 한 번만 담아 두는 것**이 중요하다는 걸 배웠다.

### 배운 점

- `list_max`의 비교 함수는 **"a < b 이면 true"** 를 돌려줘야 한다. 이름은 `max`지만 비교 함수는 "작다"를 판단한다.
- 우선순위가 **같을 때는 양보하지 않는다.** 그래야 같은 우선순위끼리는 들어온 순서(FIFO)가 지켜진다. (`priority-fifo`)
- 지금은 `ready_list`를 정렬하지 않고 꺼낼 때 `list_max`로 찾는다. 나중에 Donation으로 **줄에 들어간 뒤 우선순위가 바뀌는 경우**(C-2)를 생각하면 꺼낼 때 찾는 방식이 오히려 안전하다.

---

## 3. 세마포어 — 맛집 하나로 이해하기

발표 준비를 하면서 세마포어를 **맛집 비유 하나로** 정리했다. 함수 이름보다 개념을 먼저 잡는 게 목표였다.

### 비유 규칙

| 맛집 | 세마포어 |
|---|---|
| 손님 | 스레드 |
| 테이블 | 자원 |
| **빈 테이블 수** | **S** (0 이상의 정수) |
| 웨이팅 명단 | 대기 큐 |
| "테이블 주세요" | **P** (`sema_down`) |
| "테이블 비었어요" | **V** (`sema_up`) |

> 암기용: **P = Please (주세요)**, **V = Vacant (비었어요)**

### 두 연산이 확인하는 것

| 연산 | 확인 | 결과 |
|---|---|---|
| **P** | **빈 자리 있나?** (S > 0) | 있으면 S − 1 하고 앉음 / 없으면 대기표 받고 **BLOCKED** |
| **V** | **기다리는 사람 있나?** | 있으면 맨 앞 손님에게 자리 넘김 (**S 그대로**) / 없으면 **S + 1** |

- 검산식: **앉아 있는 손님 수 + S = 전체 테이블 수**
- V를 한 쪽은 **절대 기다리지 않는다.**

### 스레드 상태도 맛집으로 (안내 직원 = CPU, 1명)

| 상태 | 맛집 |
|---|---|
| **RUNNING** | 직원이 지금 나를 응대 중 |
| **READY** | 직원 응대 줄에 서 있음 (직원이 오면 바로 진행 가능) |
| **BLOCKED** | 대기표 받고 테이블을 기다림 (직원이 와도 할 게 없음) |

→ **직원(CPU)을 기다리면 READY, 테이블(자원)을 기다리면 BLOCKED.**

### 세마포어의 4가지 성질

**① 원자성** — P의 "확인 → 가져가기"는 한 덩어리여야 한다.
끊기면 빈 테이블 1개에 두 팀이 동시에 "있네!" 하고 앉는다(S = −1). 컴퓨터는 경고하지 않고 **결과만 조용히 틀린다.** Pintos에서는 `intr_disable()`로 보장한다.

**② 잠들어서 기다린다 (No busy waiting)** — 문 앞에서 "자리 났어요?"를 계속 묻는 대신 명단에 적고 쉰다.
바쁜 대기가 진짜 손해인 이유는 **자기가 기다리는 일을 자기가 늦추기 때문**이다. S를 1로 만드는 건 자원을 쓰던 스레드의 V인데, 그 스레드도 CPU가 있어야 V를 할 수 있다. 우선순위 스케줄링에서는 더 심하다. 높은 스레드가 바쁜 대기를 하면 계속 자기가 다시 뽑혀서, 낮은 스레드는 **영원히** V를 못 한다.

**③ 신호를 기억한다** — 포장 픽업대(S = 0에서 시작).
주방이 먼저 음식을 내놓으면(V) 픽업대에 **보관**(S = 1)되고, 나중에 온 손님은 기다리지 않고 바로 가져간다(P). 기억하지 못하는 "외치기" 방식이라면 아무도 없을 때 외친 신호는 사라지고, 나중에 온 손님은 영원히 기다린다 → **lost wakeup**.

**④ 소유자가 없다** — 누가 P를 했는지 **기록하지 않는다.** 그래서 누구든 V를 할 수 있다.
- 장점: 손님(P)과 주방(V)처럼 **다른 스레드끼리 신호**를 주고받을 수 있다.
- 위험: 아직 식사 중인데 지나가던 사람이 "비었어요"를 외칠 수 있다.
- 그래서 **임계 구역에는 락**(잠근 본인만 해제), **신호 보내기에는 세마포어**를 쓴다.

### 같은 S, 두 가지 쓰임새

| | 자원 나눠 쓰기 (매장) | 신호 보내기 (포장) |
|---|---|---|
| 시작 S | 자원 개수 N (락이면 1) | **0** |
| S가 세는 것 | 남은 자원 | 일어났지만 아직 안 가져간 사건 |
| V를 하는 쪽 | P를 했던 **그 스레드** | **다른 스레드** |
| Pintos 예 | `struct lock` 안의 세마포어 | 이후 userprog의 exec / wait |

### 🧭 락? 세마포어? 고르는 법

```
질문 1. 같은 자원이 N개인가?
   └ 예 → 세마포어 (S = N)
   └ 아니오 ↓
질문 2. P를 하는 스레드와 V를 하는 스레드가 같은가?
   └ 같다 (내가 잠그고 내가 푼다) → 락
   └ 다르다 (A가 기다리고 B가 알려 준다) → 세마포어 (S = 0)
```

문장 단서: "N개 나눠 쓰기" → 세마포어(N) / "한 번에 하나만" → 락 / **"~가 끝나면 ~하기"** → 세마포어(0)

### Priority Donation이 락에만 있는 이유

- **우선순위 역전**: 낮은 A(20)가 락을 쥐고 있고 높은 B(63)가 그 락을 기다리는데, 상관없는 C(40)가 A를 밀어내서 B까지 C를 기다리게 되는 상황.
- **Donation**: B가 A에게 63을 빌려줘서 A가 빨리 끝내고 락을 놓게 한다.
- 그런데 빌려주려면 **누가 자원을 쥐고 있는지** 알아야 한다. 락은 `holder`를 기록하지만, 세마포어는 기록하지 않는다.
- → **Donation은 소유자가 있는 락에만 적용된다.**

> 발표용 한 줄: **"낮은 스레드가 쥔 자원을 높은 스레드가 기다리면, 중간 스레드에게 밀려 우선순위 역전이 생긴다. Donation으로 막는다. 단, 소유자가 있는 락에만 쓸 수 있다."**

### 개념 ↔ Pintos 대응

| 개념 | Pintos |
|---|---|
| S | `sema->value` |
| 대기 큐 | `sema->waiters` |
| P / V | `sema_down()` / `sema_up()` |
| 원자성 보장 | `intr_disable()` ~ `intr_set_level()` |
| 잠들기 / 깨우기 | `thread_block()` / `thread_unblock()` |
| 락 = 이진 세마포어 + 소유자 | `struct lock { holder; semaphore; }` |

---

## 4. 이번 주에 가장 많이 헷갈렸던 것

복습 퀴즈를 풀면서 **반복해서 틀린 것**들이다.

| 오해 | 바로잡기 |
|---|---|
| P = 기다리는 사람, V = 쓰고 있는 사람 | ❌ P/V는 **역할이 아니라 한 순간의 동작**이다. P는 "주세요" 하는 순간, V는 "비었어요" 하는 순간. "기다리는 중"은 동작이 아니라 **상태(BLOCKED)** |
| P를 하면 무조건 기다린다 | ❌ P는 **요청**이다. 기다림은 가져갈 게 없을 때만 생긴다 |
| 대기표 들고 기다리면 READY | ❌ **BLOCKED**. READY는 CPU를 기다리는 상태 |
| V로 호출받으면 바로 RUNNING | ❌ **READY**. 스케줄러가 골라 줘야 RUNNING |
| S를 1로 만드는 건 스케줄러 / 타이머 | ❌ **자원을 쓰던 스레드의 V**. "그 값을 바꾸는 코드를 누가 실행하나?"를 먼저 생각하기 |
| 락은 사용 중이니까 S = 0으로 시작 | ❌ 락은 **열린 채로 시작**(S = 1). 누가 쓰는 동안만 0 |
| "다운로드 끝나면 재생"은 락 | ❌ 기다리는 쪽과 알려 주는 쪽이 다르다 → **세마포어(S = 0)** |
| 바쁜 대기 문제를 Donation으로 해결 | ❌ 바쁜 대기는 **BLOCKED로 재워서** 해결. Donation은 그다음 문제인 **우선순위 역전**을 해결 |
| 높은 B가 있는데 낮은 A가 어떻게 자원을 먼저 갖나 | A가 **먼저 와서** 잡았다. **선점은 CPU만 뺏고 자원은 못 뺏는다** |

> 가장 큰 깨달음: **"동작"과 "상태"를 구분하자.** 표의 한 줄은 동작 하나다.

---

## 5. 회고

### 😊 잘한 점

- 구현 전에 **"무엇이 바뀌어야 하는가"** 를 먼저 표로 정리하고 시작했다. 테스트와 요구사항이 연결되니 방향을 잃지 않았다.
- 세마포어를 **비유 하나(맛집)로 통일**해서 정리했더니, 처음 듣는 사람에게도 설명할 수 있는 수준이 됐다.
- 틀린 개념을 그냥 넘기지 않고 **"헷갈렸던 점"으로 기록**했다. 같은 실수를 두 번 하면 (재발) 표시를 해서 뿌리를 찾았다.

### 😅 아쉬운 점

- 개념 정리에 시간을 많이 써서 **Priority Donation(B)** 구현까지 가지 못했다.
- lock / semaphore / condition variable에서 **기다리던 스레드를 우선순위 순으로 깨우기(A-4)** 도 아직 남아 있다.

### 🚀 다음 주에 할 것

- [ ] `sema_up` / `cond_signal`에서 가장 높은 우선순위 스레드부터 깨우기 (`priority-sema`, `priority-condvar`)
- [ ] Priority Donation: 단일 → 여러 기부 → 연쇄 기부 (`priority-donate-*`)
- [ ] 기부 회수 후 즉시 양보 등 A와 B가 맞물리는 지점 처리 (`priority-donate-sema`)
- [ ] 여유가 되면 MLFQS

---

## 참고

- KAIST Pintos GitBook — Project 1: Threads, Appendix: Synchronization
- `pintos/tests/threads/` 테스트 코드와 `Rubric.priority`
