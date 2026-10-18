---
tags: [pintos, project2, userprog, 시스템콜, 팀분담, 일정]
topic: Project 2 — 3인 분담 타임라인 (10/18 일 ~ 10/21 수)
status: 팀 계획 문서(PDF 15쪽) 요약. 앞으로 이 방향으로 진행
---

# 🗺️ Project 2 — 3인 분담 타임라인 (10/18 ~ 10/21)

> 원본: 팀 계획 문서 PDF(15쪽, 10/18 작성) 요약.
> 인자 전달(Argument Passing)은 팀 `main`에 이미 머지된 상태에서 시작한다.

## 0. 한 줄 요약

**일요일 밤에 C(민희)가 뼈대를 `main`에 올리고 → 월요일 오전에 `process_wait`과 종료 메시지를 머지해 테스트 PASS를 보고 → 화요일 저녁까지 기능을 다 끝내고 → 수요일은 새 기능 없이 해제 누락·동시성만 잡아 `make check` 전체 통과.**

---

## 1. 역할

| 파트 | 담당 | 맡는 범위 | 난도 |
|---|---|---|---|
| **C** | 민희 | 뼈대(공통 부분) + `process_wait` + fork | 가장 어려움 |
| **A** | 상균 | `halt`·`exit`·`write`(콘솔), 종료 메시지, 유저 포인터 검증, `page_fault`, exec | 중간 |
| **B** | 준우 | fd 테이블, 파일 시스템 콜 9개, rox | 양이 가장 많음 |

- 완료: 인자 전달 (`load()`에서 커맨드라인 분리, 유저 스택 적재, `rdi`/`rsi` 설정)
- 남은 것: 시스템 콜 전부, 종료 메시지, `process_wait`, fork, exec, rox, 자원 정리

## 2. 일정 한눈에

| 날짜 | C (민희) | A (상균) | B (준우) |
|---|---|---|---|
| 10/18 일 밤 | 뼈대 → `main`, `process_wait` | 준비 학습 | 준비 학습 |
| 10/19 월 | wait 예외 처리, fork 시작 | `exit`·종료 메시지 → 검증 함수, `page_fault` | fd 테이블, `create` `remove` `open` `close` |
| 10/20 화 | fork 마무리, fd 복제 | exec → C 지원 | `filesize` `read` `write` `seek` `tell`, rox |
| 10/21 수 | 셋이 같이 통합 점검 | ← | ← |

### 꼭 지켜야 할 순서 3가지 (병목)

1. **뼈대가 `main`에 올라가야** A·B가 시작할 수 있다.
2. **A의 종료 메시지 + C의 `process_wait`** 이 머지돼야 누구든 테스트 PASS를 볼 수 있다 → **월요일 오전 안에** 둘 다 머지.
3. **B의 fd 테이블** 이 머지돼야 C가 fork에서 fd를 복제할 수 있다 → **월요일 안에** 머지.

```
          10/18 일 밤        10/19 월                10/20 화            10/21 수
C(민희)   뼈대→main ──┬──▶ process_wait(오전 머지) ──▶ fd 복제/fork 마무리 ─┐
                     │      → fork               ┌──▶                      │
A(상균)   준비 학습   ├──▶ exit·종료 메시지(오전) │    exec → C 지원  ───────┼──▶ 통합 점검
                     │      → 검증               │                         │   multi-oom
B(준우)   준비 학습   └──▶ fd 테이블(저녁 머지) ──┘    read·write·rox·fd정리 ┘   make check / 태그
```

---

## 3. 용어 정리 (막히면 여기부터)

| 용어 | 뜻 | 핀토스에서 어디 |
|---|---|---|
| 시스템 콜 | 유저 프로그램이 커널에 부탁하는 창구 | `userprog/syscall.c`의 `syscall_handler()` |
| 시스템 콜 번호 | 어떤 부탁인지. `rax`로 들어옴 | `include/lib/syscall-nr.h` (`SYS_HALT`, `SYS_EXIT` …) |
| 시스템 콜 인자 | 순서대로 `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9` | `f->R.rdi` 등 |
| 반환값 | `rax`에 써 넣는다 | `f->R.rax` |
| `intr_frame` | 커널 진입 시 저장한 유저 레지스터 전부 | `include/threads/interrupt.h` |
| 종료 메시지 | `이름: exit(상태)` 한 줄. 테스트가 이걸로 채점 | A 담당 |
| 종료 상태 | `exit(3)`의 3. 부모가 `wait`로 받음 | `struct thread` 필드 |
| 세마포어 | `sema_down` = 기다림, `sema_up` = 깨움 | `include/threads/synch.h` |
| 락 | `lock_acquire` / `lock_release` | 파일 시스템 락 |
| fd | 0 = 키보드, 1 = 화면, 2부터 실제 파일 | B 담당 |
| 유저 포인터 검증 | NULL·커널 영역·매핑 안 된 주소면 `exit(-1)` | A 담당 |
| page fault | 매핑 안 된 주소 접근 시 CPU가 알리는 예외 | `userprog/exception.c`의 `page_fault()` |
| fork | 현재 프로세스 복사. 자식은 0, 부모는 자식 pid | `process_fork()`, `__do_fork()` |
| exec | 현재 프로세스를 다른 프로그램으로 교체 | `process_exec()` |
| wait | 자식 종료까지 기다렸다 종료 상태 받기 | `process_wait()` |
| rox | 실행 중인 프로그램 파일에 쓰기 금지 | `file_deny_write()` |
| 회귀 | 새 기능 넣다가 예전 통과 테스트가 깨지는 것 | `make check`로 확인 |

---

## 4. 10/18 (일) 밤 — 뼈대

목표: 뼈대를 `main`에 올려 **내일 아침 A·B가 바로 코딩 시작**. A·B는 코드 없이 준비 학습만.

### C (민희): 뼈대 = 셋이 같이 쓰는 이름과 자리만 박아 둔다 (본문 구현 X)

| 시간 | 할 일 | 파일 |
|---|---|---|
| 18:00~18:40 | `struct thread`에 필드 추가 (`#ifdef USERPROG` 안, 필드마다 한국어 주석 + 쓰는 기능) | `include/threads/thread.h` |
| 18:40~19:00 | 새 필드 초기화 (리스트·세마포어 init, 종료 상태 기본값) | `threads/thread.c`의 `init_thread()` |
| 19:00~19:30 | `syscall_handler`에 `switch (f->R.rax)` 골격. `SYS_HALT`~`SYS_CLOSE` 마다 `sys_*` 호출, `default`는 종료 | `userprog/syscall.c` |
| 19:30~19:45 | `sys_*` 14개, 검증 함수 3개, fd 함수 2개 **선언만** | `include/userprog/syscall.h`, `syscall.c` |
| 19:45~20:00 | 파일 시스템 락: `syscall.c`에 선언, `syscall.h`에 공개, `syscall_init()`에서 초기화 (`process.c`의 `load`에서도 쓰므로 `static` 아님) | `syscall.c`, `syscall.h` |
| 20:00~20:15 | `make` → `make check`로 이전과 결과 같은지 확인 | |
| 20:15 | 커밋 → `main` 반영 → 팀 채널에 "뼈대 올림, 최신 `main`에서 브랜치 따세요" | |

**`struct thread` 필드 후보** (snake_case, 최종 결정은 민희)
- 종료 상태 (exit·wait)
- 자식 목록 + 자식 목록용 `list_elem` (wait·fork)
- 종료 대기용 세마포어 — 부모가 자식 **종료**를 기다림 (wait)
- fork 완료 대기용 세마포어 — 부모가 자식 **복제 완료**를 기다림 (fork)
- fd 테이블, 다음에 줄 fd 번호 (fd)
- 실행 중인 파일 (rox)

**뼈대에서 같이 정해 둘 것** (팀 채널에 한 줄씩 공유)
- [ ] 검증 함수 3개 이름·인자: 주소 1개 / 버퍼(주소 + 크기) / 문자열
- [ ] fd 함수 2개 이름·인자: 파일 받아 fd 할당 / fd로 파일 찾기
- [ ] fd 테이블 크기 상수(`FD_MAX`)와 값. `struct thread` 안 배열로 둘지, 페이지를 따로 받을지
  - ⚠️ `struct thread`는 커널 스택과 같은 4KB 페이지에 있어서, 배열이 크면 커널 스택이 그만큼 줄어든다
- [ ] 종료 메시지 출력 위치: 정상 `exit`와 커널 강제 종료가 둘 다 지나가는 함수 (A 구현)
- [ ] `process_exit()` 안에서 A·B·C 블록 나누기 (7절)

### C (민희): `process_wait` (뼈대 이후, 여력만큼 — 못 끝내면 월요일 오전 첫 작업)

| 순서 | 할 일 | 확인 |
|---|---|---|
| 1 | 자식을 만들 때 부모의 자식 목록에 등록 (`process_create_initd`, 이후 fork에서도) | 자식 목록 출력 |
| 2 | `process_wait(tid)`: 자식 목록에서 tid로 찾기 | 못 찾으면 -1 |
| 3 | 자식이 끝날 때까지 종료 대기 세마포어로 기다림 | `Executing …` 뒤 `system call!` 보이고 핀토스 꺼짐 |
| 4 | 자식은 종료할 때(`process_exit`) 부모를 깨움 | 3과 같이 확인 |
| 5 | 종료 상태 받아 반환, 자식 목록에서 빼고 자식 정보 해제 | A 종료 메시지와 합쳐지면 `args-*` PASS |

- **짜기 전에 정할 것:** 자식이 먼저 끝나면 종료 상태는 어디에 남아 있어야 하나? 부모가 먼저 끝나면 자식 정보는 누가 해제하나? 자식 스레드 구조체가 해제된 뒤 부모가 종료 상태를 읽으면 안 된다.
- 지금 임시로 넣어 둔 `while (1);`은 이 작업에서 지운다.

### A (상균): 준비 학습 (코드 없음)
- [ ] KAIST 문서 System Calls 앞부분: 시스템 콜이 커널 어디로 들어오는지, 인자가 어느 레지스터에 오는지
- [ ] `halt`, `exit`, `write` 명세. `write`는 fd 1이면 `putbuf()`를 한 번에 호출
- [ ] Process Termination Messages: 출력 형식, 출력하지 않는 경우 (커널 스레드, `halt`)
- [ ] `process_create_initd()`에서 스레드 이름이 커맨드라인 전체로 들어감 → 종료 메시지는 프로그램 이름만이어야 하므로 어디서 자를지
- [ ] `page_fault()` 읽고 유저 코드에서 난 fault를 어떻게 구분하는지
- [ ] `is_user_vaddr()`(`vaddr.h`), `pml4_get_page()`(`mmu.c`)가 무엇을 해 주는지

### B (준우): 준비 학습 (코드 없음)
- [ ] `filesys.h`, `file.h` 함수 정리: `filesys_create`, `filesys_remove`, `filesys_open`, `file_close`, `file_length`, `file_read`, `file_write`, `file_seek`, `file_tell`
- [ ] System Calls의 `create`~`close` 명세. 실패하면 각각 무엇을 반환하는지
- [ ] fd 테이블 구조 구상 (크기, 빈 칸 찾기, 닫을 때 처리) → 뼈대 결정에 의견
- [ ] fd 0 읽기 = `input_getc()`, fd 1 쓰기 = `putbuf()`
- [ ] Denying Writes to Executables 읽기 (화요일 rox)

---

## 5. 10/19 (월)

목표: **오전 안에** A 종료 메시지 + C `process_wait` 머지 → 모두 테스트 가능. **저녁까지** B fd 테이블 머지.

### 하루 흐름 (셋 공통, 화요일도 동일)

| 시간 | 내용 |
|---|---|
| 10:00~10:15 | 짧은 회의: 어제 한 것, 오늘 할 것, 막힌 것 |
| 10:15~12:00 | 오전 작업. 오전 머지 대상은 끝나는 즉시 PR |
| 13:00~19:00 | 오후 작업 |
| 19:00~21:00 | 저녁 작업, PR 올리기 |
| 21:00~22:00 | 서로 PR 리뷰 → 머지 → 전원 최신 `main` 받기 |

### C (민희)

| 시간 | 할 일 | 확인 테스트 |
|---|---|---|
| 10:15~12:00 | `process_wait` 마무리 → **PR → 머지** | A 머지 후 `args-*`, `exit` |
| 13:00~15:00 | wait 예외: 직계 자식 아닌 tid → -1, 같은 자식 두 번 wait → 두 번째 -1, 커널이 강제 종료한 자식 → -1 | `wait-simple`, `wait-twice`, `wait-bad-pid`, `wait-killed` |
| 15:00~17:00 | fork 1: `sys_fork`에서 `process_fork()` 호출. 부모의 `intr_frame`을 자식에게 넘길 방법 정하기. 자식은 `__do_fork()`에서 복사하고 `rax = 0` | 자식 스레드 생성 |
| 17:00~19:00 | fork 2: `duplicate_pte()` 구현 (커널 주소면 건너뜀, 새 유저 페이지 받기, 부모 페이지 복사, 쓰기 가능 여부 유지, 자식 페이지 테이블에 등록) | `fork-once`에서 자식 실행 |
| 19:00~21:00 | fork 3: 부모는 fork 완료 대기 세마포어로 복제 끝날 때까지 기다림. 성공 → 자식 tid, 실패 → `TID_ERROR`. 자식을 자식 목록에 등록 | `fork-once`, `fork-multiple` |

- "커널 강제 종료"는 A의 `page_fault` 처리에서 종료 상태 -1을 남겨야 확인됨 → A와 맞춘다
- **fork 대기와 wait 대기는 목적이 다르다** (복제 완료 vs 종료). 세마포어를 섞어 쓰지 않는다
- 막히면 30분 안에 팀 채널 공유. fork는 화요일에 A가 합류

### A (상균)

| 시간 | 할 일 | 확인 테스트 |
|---|---|---|
| 10:15~10:45 | `sys_halt`: `power_off()` | `halt` |
| 10:45~11:15 | `sys_write`: fd 1이면 `putbuf()` 한 번, 쓴 바이트 수를 `rax`에. 아니면 B 몫으로 비워 둠 | `(args) begin` 같은 출력 |
| 11:15~11:40 | `sys_exit`: 종료 상태를 뼈대 필드에 저장하고 `thread_exit()` | |
| 11:40~12:00 | 종료 메시지 `프로그램이름: exit(상태)` 한 번만. 커널 스레드는 출력 X. 이름은 첫 단어만 → **PR → 머지** | `args-*`, `halt`, `exit` (C의 wait과 함께) |
| 13:00~15:00 | 검증 함수 1: 주소 하나 (NULL, `!is_user_vaddr`, `pml4_get_page == NULL`이면 `exit(-1)`) | `*-null`, `*-bad-ptr` |
| 15:00~17:00 | 검증 함수 2: 버퍼(시작 ~ 시작+크기-1), 문자열(`\0`까지). 페이지 경계 포함. `sys_write`에 적용 | `*-boundary`, `sc-*` |
| 17:00~19:00 | `page_fault()`: 유저 코드 fault면 종료 상태 -1로 종료 | `bad-read`, `bad-write`, `bad-jump` |
| 19:00~21:00 | 회귀 확인 → **PR** | 기존 PASS 유지 |

- 검증 실패로 `exit(-1)` 할 때 **파일 시스템 락을 잡고 있으면 안 된다** → B와 "락 잡기 전에 검증"으로 맞춤
- 확인용 `printf`가 하나라도 남으면 **모든 테스트 FAIL**

### B (준우)

| 시간 | 할 일 | 확인 테스트 |
|---|---|---|
| 10:15~12:00 | fd 테이블: 프로세스 시작 시 준비(0·1은 콘솔용으로 비움), fd 할당 함수, fd로 파일 찾기(없거나 범위 밖이면 NULL) | 컴파일 |
| 13:00~15:00 | `sys_create`, `sys_remove`: 락 안에서 `filesys_create`/`filesys_remove`, 성공 여부 반환 | `create-normal`, `create-empty`, `create-long`, `create-exists` |
| 15:00~17:00 | `sys_open`: 락 안에서 `filesys_open`, 성공 시 fd, 실패 -1. 같은 파일 두 번 열면 다른 fd | `open-normal`, `open-missing`, `open-twice` |
| 17:00~18:30 | `sys_close`: 락 안에서 `file_close`, fd 칸 비움. 없는/이미 닫은 fd는 무시 또는 종료(명세 확인) | `close-normal`, `close-twice`, `close-bad-fd` |
| 18:30~19:00 | fd 테이블 부분 먼저 **PR → 머지** (C의 fork fd 복제가 기다림) | |
| 19:00~21:00 | A 검증 함수 머지되면 `create`, `open` 파일 이름에 문자열 검증 붙이기 | `create-null`, `create-bad-ptr`, `open-null`, `open-bad-ptr` |

- 모든 `filesys_*`, `file_*` 호출은 락 안에서. **락 잡은 채로 `return`/`exit` 하는 경로가 없어야** 한다
- 검증 함수 머지 전에는 정상 입력 테스트부터 통과

---

## 6. 10/20 (화)

목표: **저녁까지 기능 구현 전부 끝내고 `main`에 머지.**

### C (민희)

| 시간 | 할 일 | 확인 테스트 |
|---|---|---|
| 10:15~12:00 | fd 복제: `__do_fork()`에서 부모가 연 파일을 자식도 같은 상태로 (`file_duplicate()`, 락 안에서). B fd 테이블 머지 후 시작 | `fork-read`, `fork-close` |
| 13:00~15:00 | fork 실패 경로: 복제 실패 시 자식은 종료, 부모는 깨어나서 `TID_ERROR`. 부모가 영원히 기다리는 경로 없는지 | `fork-*` 전체 |
| 15:00~17:00 | 해제 경로: 부모가 먼저 끝날 때 자식 정보 해제, 자식이 먼저 끝날 때 종료 상태 보존. A와 같이 | `wait-*`, `fork-recursive` |
| 17:00~19:00 | rox가 fork된 자식에서도 유지되는지 B와 확인 | `rox-child`, `rox-multichild` |
| 19:00~21:00 | 회귀 확인 (`make check` 전체) → **PR** | 이전 PASS 유지 |

### A (상균)

| 시간 | 할 일 | 확인 테스트 |
|---|---|---|
| 10:15~13:00 | `sys_exec`: 커맨드라인 문자열 검증 → **새 페이지에 복사** (`process_exec`가 현재 주소 공간을 지우므로 유저 메모리 원본은 그 뒤에 못 읽음) → `process_exec()`. 실패하면 -1로 종료, 성공하면 안 돌아옴. fd와 스레드 이름 유지 | `exec-once`, `exec-arg`, `exec-boundary`, `exec-missing`, `exec-bad-ptr` |
| 13:00~15:00 | 종료 메시지가 모든 종료 경로(정상 `exit`, `page_fault`, 검증 실패, exec 실패)에서 정확히 한 번 찍히는지 | 모든 `.ck` 마지막 줄 |
| 15:00~21:00 | C와 합류: fork·wait 남은 테스트, 해제 경로 점검 | `fork-*`, `wait-*` |

### B (준우)

| 시간 | 할 일 | 확인 테스트 |
|---|---|---|
| 10:15~11:30 | `sys_filesize`, `sys_seek`, `sys_tell`: 락 안에서 `file_length`/`file_seek`/`file_tell`. 없는 fd는 -1 또는 무시 | |
| 11:30~13:00 | `sys_read`: 버퍼 검증 → fd 0이면 `input_getc()`로 한 글자씩, fd 1이면 실패, 파일이면 락 안에서 `file_read`. 읽은 바이트 수 반환 | `read-normal`, `read-zero`, `read-stdout`, `read-bad-fd`, `read-boundary`, `read-bad-ptr` |
| 14:00~15:30 | `sys_write` 파일 부분: 버퍼 검증 → fd 0이면 실패, 파일이면 락 안에서 `file_write`. fd 1은 A 코드 그대로 | `write-normal`, `write-zero`, `write-stdin`, `write-bad-fd`, `write-boundary`, `write-bad-ptr` |
| 15:30~18:00 | rox: `load()` 끝에서 실행 파일을 닫지 않고 스레드 필드에 보관 + `file_deny_write()`. 종료 시 닫아서 쓰기 다시 허용 | `rox-simple`, `rox-child`, `rox-multichild` |
| 18:00~19:00 | 종료 시 열린 fd 전부 닫기, fd 테이블 해제 (`process_exit`) | |
| 19:00~21:00 | 회귀 확인 (`make check` 전체) → **PR** | 이전 PASS 유지 |

- rox: `load()`의 `done:` 아래 `file_close(file);` 처리 결정 필요 — **로드 실패면 닫고, 성공이면 열어 둔다**
- `filesize`~`tell`은 짧으니 오전에 끝내고 `read`/`write` 예외 케이스에 시간 쓰기

### 화요일 저녁 머지
- 21:00~22:00: 세 PR 리뷰 후 머지. **순서는 B → C → A** (B의 fd 정리와 rox가 C·A 테스트에 영향)
- 머지 후 `make check` 한 번 돌려 실패 목록을 팀 채널에 → 수요일 아침 출발점

---

## 7. 10/21 (수) — 셋이 같이 통합 점검

새 기능 X. **해제 누락과 동시성 문제를 잡아 `make check` 전체 통과.**

| 시간 | 할 일 | 확인 |
|---|---|---|
| 10:00~10:15 | 화요일 밤 실패 목록 보고 담당 나누기 (원래 그 기능 짠 사람이 원인 분석) | |
| 10:15~12:00 | 종료 경로 점검: 정상 `exit`, 커널 강제 종료, 로드 실패, fork 실패 각각에서 아래 항목이 모두 해제되는지 코드 따라가며 확인 | 경로별 체크 |
| 13:00~15:00 | 해제 누락 잡기 → `multi-oom`. 누락이 하나만 있어도 실패하고 출력에 원인이 안 나오므로 위 점검을 먼저 끝낸다 | `multi-oom` |
| 15:00~17:00 | 동시성: 여러 프로세스가 동시에 파일 시스템 콜 | `filesys/base`의 `syn-*`, `lg-*`, `sm-*` |
| 17:00~19:00 | `make check` 전체 → 남은 실패는 그 테스트의 `.c`와 `.ck`만 보고 원인 찾기 | 전체 PASS |
| 19:00~ | 디버깅 출력 제거 확인, 최종 머지, `project2-final` 태그 | |

### 종료할 때 해제해야 하는 것 (경로마다 전부 확인)
- [ ] 열린 파일 전부 (fd 2 이상) — B
- [ ] fd 테이블 자체 — B
- [ ] 실행 파일 (닫으면 쓰기 다시 허용) — B
- [ ] 자식 정보 (부모가 먼저 끝난 경우 포함) — C
- [ ] 잡고 있던 파일 시스템 락 — A·B
- [ ] 페이지 테이블 (원래 코드 `process_cleanup()`이 처리) — 확인만

### 경로별 체크표

| 경로 | 열린 파일 | fd 테이블 | 실행 파일 | 자식 정보 | 락 |
|---|---|---|---|---|---|
| 정상 `exit` | | | | | |
| 커널 강제 종료 (`page_fault`, 검증 실패) | | | | | |
| 로드 실패 | | | | | |
| fork 실패 (자식 쪽) | | | | | |

---

## 8. 겹치는 파일과 경계

| 파일 / 함수 | A (상균) | B (준우) | C (민희) |
|---|---|---|---|
| `include/threads/thread.h` | 뼈대 필드 사용만 | 뼈대 필드 사용만 | 뼈대 작성 |
| `userprog/syscall.c` | `sys_halt`, `sys_exit`, `sys_write`(fd 1), `sys_exec`, 검증 함수 | `sys_create`~`sys_close`, `sys_write`(파일), fd 함수 | `sys_fork`, `sys_wait`, `switch` 골격 |
| `process.c`의 `process_exit()` | 종료 메시지 출력 | 열린 파일·fd 테이블·실행 파일 닫기 | 부모 깨우기, 자식 정보 해제 |
| `process.c`의 `load()` | | 실행 파일 열어 두기 (rox) | |
| `process.c`의 `process_create_initd()` | 스레드 이름 자르기 | | 자식 목록 등록 |
| `process.c`의 fork 관련 | | | `process_fork`, `__do_fork`, `duplicate_pte` |
| `userprog/exception.c` | `page_fault()` | | |

### `process_exit()` 블록 나누기 규칙
- 세 사람이 같은 함수를 고치므로 **각자 블록을 따로** 두고 블록 위에 한국어 주석
  (예: `/* 종료 메시지. */`, `/* 열린 파일 정리. rox · fd. */`, `/* 부모 깨우기와 자식 정리. wait. */`)
- **순서: 종료 메시지 → 파일 정리 → 부모 깨우기.** 부모를 깨우는 게 마지막이어야 부모가 정리 중인 자식을 읽지 않는다
- 남이 머지하면 바로 최신 `main`을 받아 충돌을 작게 유지

---

## 9. 공통 규칙

### 코드
- 탭 들여쓰기, 함수 이름 뒤 한 칸 (`thread_yield ();`), 반환형은 윗줄
- snake_case, 상수만 대문자. 시스템 콜 구현 함수는 `sys_`로 시작
- 새 주석은 한국어 `/* */`, "무엇"보다 "왜". 원래 영어 주석은 건드리지 않음
- 뼈대에서 정한 이름·필드만 사용. 더 필요하면 팀 채널에 먼저 말하고 추가
- `#ifdef VM` 안, `filesys/`, `tests/`는 건드리지 않음

### 동기화
- `filesys_*`, `file_*` 호출은 전부 파일 시스템 락 안에서
- **락 잡은 채로 `return`/`exit` 금지. 유저 포인터 검증은 락 잡기 전에**
- `sema_up()`에 `thread_yield()`를 넣지 않는다 (디스크 인터럽트 안에서 불려서 패닉)

### 브랜치와 머지
- 브랜치 이름: `feat/기능/이니셜` (예: `feat/wait/kmh`, `feat/file/abc`)
- 기능 하나 = 브랜치 하나 = PR 하나. 리뷰어는 나머지 두 명, **Squash and merge**
- 상대가 머지하면 바로 `git pull origin main`으로 반영
- 이전 PASS 테스트가 FAIL이 되면 머지하지 않음
- PR 설명에 새로 통과한 테스트와 깨진 테스트 **이름**을 적는다
- 리뷰어는 동기화·해제 시점·유저 포인터 처리를 **자기 말로 설명할 수 있을 때** 승인 (AI로 짠 코드일수록 꼭)

### 브랜치 계획 (미리 만들지 않는다 — 뼈대 머지 후 최신 `main`에서 하나씩)

| 담당 | 순서대로 쓸 브랜치 |
|---|---|
| 민희 (C) | `feat/skeleton/kmh` → `feat/wait/kmh` → `feat/fork/kmh` |
| 상균 (A) | `feat/syscall-basic/이니셜` → `feat/user-memory/이니셜` → `feat/exec/이니셜` |
| 준우 (B) | `feat/fd/이니셜` → `feat/file-syscall/이니셜` → `feat/rox/이니셜` |

1. 기능 하나 끝나면 PR → 리뷰 → Squash and merge
2. 머지된 브랜치는 지운다
3. 다음 브랜치는 최신 `main`에서: `git checkout main` → `git pull origin main` → `git checkout -b feat/기능/이니셜`
4. 작업 중 다른 사람이 머지하면 `git pull origin main`으로 반영, 충돌은 그 자리에서 해결

- [ ] 민희: `feat/skeleton/kmh` 뼈대 머지 후 팀 채널에 "뼈대 머지됨, 각자 첫 브랜치 따세요"
- [ ] GitHub 저장소에 `main` 브랜치 보호 규칙 (PR 없이 직접 푸시 금지, 리뷰 1명 이상)

### 막혔을 때
- 30분 넘게 진전이 없으면 팀 채널에 **실행 명령, 출력(패닉 메시지 포함), 의심 위치**를 같이 올린다
- 패닉 콜스택은 `backtrace`로 변환해 첨부

### 실행 명령 (`pintos/userprog/build`에서)

| 목적 | 명령 |
|---|---|
| 빌드 | `make` |
| 테스트 1개, 화면으로 | `pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'` |
| 테스트 1개, 결과 파일로 | `make tests/userprog/args-single.result` |
| 전체 테스트 | `make check` |
| 결과 지우고 다시 | `rm -f tests/userprog/*.result` 후 다시 실행 |
| 최신 `main` 받기 | `git checkout main && git pull origin main` |
| 새 브랜치 | `git checkout -b feat/기능/이니셜` |
| 올리기 | `git add 파일` → `git commit` → `git push -u origin 브랜치이름` |
