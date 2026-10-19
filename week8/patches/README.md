# week8 패치 모음

팀 저장소(`Kal-Moon/JungleWeek08`)에 아직 올리지 않은 작업을 `git am` 으로 적용할 수 있게 보관한다.

| 파일 | 내용 | 적용할 기준 커밋 |
|---|---|---|
| `2026-10-19-user-memory.patch` | 10/19 오후 (A): 검증 함수 3개, `sys_write` 버퍼 검증, `page_fault` → `exit(-1)` | `a761fbf` (`feat/args/sk`, 오전 작업 + 뼈대 머지본) |

```bash
# JungleWeek08 에서
git checkout feat/args/sk && git pull
git checkout -b feat/user-memory/sk
git am /path/to/2026-10-19-user-memory.patch
```

## 2026-10-19 오후 확인 결과

- 이 패치만으로 `make check` 결과는 기준선과 **완전히 같다** (C의 `process_wait` 이 아직 `-1` 을 바로 돌려줘서 userprog 테스트는 전부 FAIL 인 상태).
- 로컬에서만 임시 `process_wait`(전역 세마포어로 자식 한 명 기다리기)을 넣고 돌리면:
  - 패치 전 userprog PASS 20개 → 패치 후 26개. **새로 PASS: `bad-read`, `bad-write`, `bad-read2`, `bad-write2`, `bad-jump`, `bad-jump2`**
  - 깨진 테스트 없음, Kernel PANIC 없음
- `*-null`, `*-bad-ptr` 은 지금은 stub 이 `exit(-1)` 해서 이미 PASS. B가 `create`·`open` 등을 구현할 때 검증 함수를 붙여야 계속 PASS 한다.
- 임시 유저 프로그램으로 직접 확인한 검증 동작 (`write(1, ...)`, 문자열은 임시로 `create` 에 연결):

| 입력 | 결과 |
|---|---|
| 페이지 경계에 걸친 정상 버퍼 / 문자열 | 통과 (출력·반환값 정상) |
| `NULL`, 커널 주소 `0x8004000000`, 매핑 안 된 `0x10123420` | `exit(-1)` |
| 스택 페이지 끝에서 시작해 매핑 밖으로 넘어가는 버퍼 | `exit(-1)` |
| `\0` 없이 스택 페이지 끝까지 가는 문자열 | `exit(-1)` |
| 크기 `0xffffffff` | `exit(-1)` |
| `write(1, NULL, 0)` | `0` 반환 (0 바이트면 버퍼를 안 읽으므로 검사 생략) |

## 팀에 공유할 것

- **B(준우):** `sys_create`·`sys_open`·`sys_remove` 는 `validate_user_string (file)`, `sys_read`·`sys_write`(파일) 는 `validate_user_buffer` 를 **락 잡기 전에** 부른다. `sys_write` 는 맨 위에서 이미 검증하므로 파일 부분엔 다시 넣을 필요 없음.
- **B(준우), 화요일 `read`:** `pml4_get_page` 는 매핑 여부만 본다. 읽기 전용 페이지(코드 영역)에 `read` 하면 커널 쓰기에서 fault 가 나므로, 필요하면 `pml4e_walk` 로 PTE 의 `PTE_W` 까지 확인하는 쓰기용 검증을 따로 만든다.
- **C(민희):** `wait-killed` 는 `page_fault` 에서 `sys_exit (-1)` 로 종료 상태가 -1 로 남는 것으로 확인 가능.
- **A(상균), 화요일 `exec`:** 커맨드라인은 `validate_user_string` 으로 검증한 뒤 새 페이지에 복사.
