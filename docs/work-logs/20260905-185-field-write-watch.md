# 20260905-185 필드 쓰기 감시점 관측 결과
# 20260905-185 Field Write Watchpoint — Results

## 1. 개요 (Overview)

EZ2DJ 4th의 결함 필드 `0x00acafc0`에 하드웨어 쓰기 감시점을 걸고 실행 전체를 관측했다.

**결론: 실행 한 번 동안 이 주소에 일어난 쓰기는 단 한 번, 생성자가 0을 넣는 것뿐이다. Task 184가 남긴 세 가설 중 "설치 경로가 아예 실행되지 않는다"만 남는다.**

**부수 확인: 정리 경로는 실행되지 않는다. 즉 만들어졌다 치워진 것이 아니라 처음부터 채워지지 않았다. 그리고 생성자의 호출자 주소를 새로 얻었다.**

A hardware write watchpoint on the faulting field observed the whole run. Exactly one write occurs: the constructor storing zero. Of the three hypotheses Task 184 left, only "the install path never runs" survives. The cleanup path never executes, so this is not a value that was installed and then torn down, and the constructor's caller is now known.

---

## 2. 변경 내용 (Changes Implemented)

`src/tools/windows_x86_launcher_probe/main.cpp`

1. **감시점 추가.** `SetFieldWriteWatch`가 `Dr1`에 4바이트 쓰기 감시점을 설정하고, `HandleFieldWriteWatchHit`이 적중을 기록한 뒤 `Dr6`만 지워 감시를 유지한다.
2. **옵션.** `--field-write-watch <hex-address>`. 4바이트 정렬을 요구하고, 정렬되지 않으면 거절한다. x86의 4바이트 데이터 중단점이 정렬을 요구하므로, 조용히 받으면 요청하지 않은 범위를 감시하게 된다.
3. **무장 범위.** 프로세스의 첫 스레드는 루프 진입 전에, 이후 생성되는 스레드는 생성 이벤트에서 무장한다. 디버그 레지스터가 스레드마다 따로이기 때문이다.
4. **레지스터 충돌 거절.** `Dr1`을 함께 쓰는 `--slot-writer-trace`, `--null-context-field-reference-execution-trace`와의 조합을 명령행에서 거절한다. 서로 덮어쓰면 두 진단 모두 거짓을 말한다.
5. **정적 `ExitProcess` import 없는 대상 허용.** 4th는 그 import를 가지지 않는다. 이 감시와 Task 184의 스캔은 관측만 하므로, 기존 bounded trace 목록에 둘을 추가해 단독으로도 실행되게 했다. 이것을 빠뜨려 첫 실행이 `requested import is not present`로 끝났다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진단 실행: `20260905-015235-070`.

### 3.1 감시점이 올바른 명령을 잡는다 (확인됨)

작업 지시서의 자기 검증 기준 그대로다.

```
{"event":"field_write_watch_ready","prepared":true,"address":"0x00acafc0","rva":"0x006cafc0"}
{"event":"field_write_watch_hit","sequence":1,"thread":44332,"value":"0x00000000",
 "eip":"0x004225db","eip_rva":"0x000225db","section":".text",
 "eax":"0x00aca5b0","ecx":"0x00aca5b0","edx":"0x00aca5b0",
 "return":"0x004a5c26","return_rva":"0x000a5c26",
 "window_base":"0x004225cb","bytes":"0000008b45fcc780100a0000000000008b4dfcc781140b00"}
```

- **확인됨 — Task 184가 찾은 저장 명령과 정확히 같다.** 스캔이 지목한 저장은 RVA `0x000225d1`의 10바이트 명령이고, 데이터 중단점은 저장이 끝난 뒤 트랩하므로 `EIP`가 `0x000225db`인 것이 정확히 일치한다.
- **확인됨 — 바이트 창이 스캔 기록과 같다.** `8b 45 fc` / `c7 80 10 0a 00 00 00 00 00 00` / `8b 4d fc` / `c7 81 14 0b 00 00`로, `mov eax,[ebp-4]`에 이어 `+0xa10`을 0으로, 다음에 `+0xb14`를 채운다. 멤버를 차례로 초기화하는 생성자라는 Task 184의 읽기가 실행으로 확인되었다.
- **확인됨 — 대상 객체가 전역 그 자체다.** `eax`, `ecx`, `edx`가 모두 `0x00aca5b0`이다.
- **새로 얻음 — 생성자의 호출자는 RVA `0x000a5c26`이다.** 정적 검색으로는 얻을 수 없던 값이다.

### 3.2 실행 전체에서 쓰기는 한 번뿐이다 (확인됨)

| 항목 | 값 |
| - | - |
| 적중 횟수 | **1** |
| 기록 상한 | 64 (도달하지 않음) |
| 무장된 스레드 | 첫 스레드 + 생성된 7개 중 6개 |
| 결함 스레드 | `44332` — 적중한 스레드와 같다 |

- **확인됨 — 가설 1과 2가 배제된다.** 하드웨어 감시점은 명령의 부호화 방식이나 실행 섹션과 무관하게 그 주소에 대한 쓰기를 잡는다. `.protect`에서 쓰든, `memcpy`로 쓰든, 계산된 주소로 쓰든, 다른 변위로 같은 주소를 쓰든 모두 트랩한다. 그런 쓰기가 하나도 없었다.
- **확인됨 — 가설 3만 남는다.** 포인터를 설치하는 경로가 이 실행에서 실행되지 않는다.
- **확인됨 — 정리 경로가 실행되지 않는다.** Task 184가 찾은 두 번째 쓰기 `0x00022aad`이 한 번도 적중하지 않았다. 객체가 만들어졌다가 해제된 뒤 쓰인 것이 아니다.
- **확인됨 — 생성과 결함이 같은 스레드다.** 스레드 간 경합의 문제가 아니다.
- **미확정 — 무장되지 않은 스레드 하나.** 첫 `create_thread`는 감시점이 준비되기 전, 로더 단계에서 발생했다. 게스트 코드가 돌기 전이며 결함 스레드도 아니지만, 그 스레드의 쓰기는 관측 범위 밖이다.

### 3.3 `.text`는 디스크에서 암호문이다 (확인됨)

추출된 실행 파일에서 `.text`는 raw offset과 가상 주소가 같으므로 RVA가 곧 파일 오프셋이다. 파일 오프셋 `0x225d1`을 읽으면 `c5 ce d8 df 49 06 93 91`이고, 실행 중 같은 위치는 `c7 80 10 0a 00 00 00 00 00 00`이다.

- **확인됨 — 정적 파일 분석으로는 이 코드를 읽을 수 없다.** 패커가 실행 중에 `.text`를 복호화한다. Task 184의 런타임 스캐너와 이번 감시점이 프로세스 메모리를 대상으로 하는 이유가 이것으로 확정된다.

### 3.4 실행 결과 표기에 대하여 (For The Record)

이 실행은 종료 코드 3, outcome `original process exited with code 0xc0000005 before ExitProcess breakpoint`로 끝났다. 게스트의 동작은 이전과 같으며 접근 위반 지점도 `eip=0x00422b3a`, `ecx=0`으로 동일하다. 단독 모드에는 이전 실행들이 쓰던 경계 처리기가 없어 outcome 문구만 다르다.

---

## 4. 남은 질문 (What Remains)

게스트는 이 멤버를 생성자에서 0으로 두고, 결함 지점에서 검사 없이 역참조한다. 그 사이에 값을 넣는 일이 **일어나지 않는다.** 정적 검색과 동적 감시가 서로 다른 방식으로 같은 답을 냈다.

따라서 다음 질문은 "무엇이 이 필드를 쓰는가"가 아니라 **"무엇이 이 객체를 만들었어야 하는가, 그리고 왜 그 코드가 실행되지 않았는가"**다.

*Static search and dynamic watch reached the same answer by different means, so the question is no longer what writes the field but what should have created the object and why that code does not run.*

---

## 5. 다음 작업 (Next Task)

1. 생성자의 호출자 RVA `0x000a5c26` 주변을 읽어, 이 전역 객체를 만드는 초기화 함수가 무엇인지 확인한다. `.text`가 암호문이므로 실행 중 메모리에서 읽어야 한다.
2. null 검사가 있는 `0x00022a5f` 계열 함수와 결함 함수 `0x00022b00`의 실행 순서를 비교한다. 두 함수 중 하나가 설치를 유발하는 조건을 가지고 있을 수 있다.
3. 이를 위해 임의 RVA에 진입 중단점을 걸어 실행 순서를 기록하는 진단이 필요하다. 기존 `--null-context-entry-trace`는 RVA가 상수로 박혀 있어 재사용할 수 없다. Task 184와 185가 스캐너·감시점에 대해 한 것과 같은 방식으로 매개변수화한다.

---

## 6. 관련 문서 (Related Documents)

- [Task 185 설계](../design/20260905-185-field-write-watch.md)
- [Task 185 작업 지시서](../work-orders/20260905-185-field-write-watch.md)
- [Task 184 작업 로그](20260905-184-guest-field-reference-scan.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)
