# 20260904-180 EZ2DJ 4th 자발적 종료 지점 관측 설계
# 20260904-180 EZ2DJ 4th Deliberate Exit Source Observation Design

## 1. 배경 및 목적 (Background & Objectives)

Task 179 이후 게스트는 더 이상 크래시하지 않는다. 자원 적재를 모두 마치고 surface 1,108개를 만든 뒤 `ExitProcess(1)`로 스스로 종료한다. 종료 직전 기록은 Hardlock descriptor IOCTL이다.

```
device-ioctl-entry:code=0x9c40244c:input_size=256:output_size=256
hardlock-device:request=descriptor:outcome=completed:bytes=256:answered=0:status_cleared=1
```

종료를 결정하는 코드 지점을 특정하는 것이 이 작업의 목적이다. 보호 검사가 원인인지, 다른 초기화 실패인지는 그 지점을 봐야 판단할 수 있다.

The guest no longer crashes; it finishes loading and exits deliberately with code 1 right after a Hardlock descriptor IOCTL. This task identifies the code that decides that exit, which is what separates a protection rejection from any other failed initialization.

---

## 2. 기존 기구가 닿지 않는 이유 (Why The Existing Machinery Does Not Reach)

런처에는 `--break-exit-process`가 있고, `ExitProcess`의 IAT slot에 breakpoint를 심어 호출자 반환 주소와 wrapper 프레임을 기록한다. 그러나 이 실행 파일은 `ExitProcess`를 **`GetProcAddress`로 동적 해석**한다. `.vfs.log`의 동적 resolver 목록에 `ExitProcess`가 있고, 정적 IAT slot 경로는 "requested import is not present"로 빠진다.

따라서 정적 slot이 아니라 동적 해석 지점에서 잡아야 한다. 주입 런타임의 `Re2djHleGetProcAddress`가 이미 이름별로 대체 함수를 돌려주고 있으므로, 그 자리에 관측용 wrapper를 추가하는 것이 자연스럽다.

The launcher's `--break-exit-process` patches a static IAT slot, but this executable resolves `ExitProcess` through `GetProcAddress`, so the static path reports the import as absent. The injected runtime's dynamic resolver is where this call can be caught.

---

## 3. 설계 (Design)

```mermaid
flowchart TD
    A["guest: GetProcAddress(\"ExitProcess\")"] --> B[Re2djHleGetProcAddress]
    B --> C[Re2djHleExitProcess wrapper]
    D["guest: ExitProcess(1)"] --> C
    C --> E["trace: code, return address, RVA, bytes before it"]
    E --> F["real ExitProcess(code)"]
```

wrapper는 관측만 하고 동작을 바꾸지 않는다. 기록하는 것은 세 가지다.

| 항목 | 이유 |
| - | - |
| 종료 코드 | 어느 경로인지 구분한다. 지금까지 관측된 값은 1이다 |
| 호출자 반환 주소와 그 RVA | 종료를 부른 코드 지점이다 |
| 반환 주소 앞 바이트 창 | 그 지점의 호출 직전 명령을 복원해 조건을 읽는다 |

동적 resolver가 이미 `ReportDynamicResolverCaller`로 같은 형태의 바이트 창을 남기고 있으므로 표현을 맞춘다.

The wrapper observes and then calls the real `ExitProcess`, recording the exit code, the caller's return address and RVA, and the bytes before it, in the same shape the dynamic resolver already uses.

---

## 4. 판정 기준 (Decision Criteria)

| 관측 | 결론 |
| - | - |
| 호출자가 Hardlock 검사 경로 안에 있다 | 보호 응답이 원인이다. 응답 material을 다음 작업에서 본다 |
| 호출자가 그래픽·사운드 초기화 실패 경로다 | 해당 facade 응답을 본다 |
| 호출자가 정상 종료 경로(창 닫힘 등)다 | 중단이 아니라 게스트가 끝난 것이다. 그 앞의 조건을 다시 본다 |
| wrapper가 한 번도 불리지 않는다 | 종료가 `ExitProcess`가 아닌 다른 경로다. `TerminateProcess`나 CRT `exit`를 본다 |

---

## 5. 비목표 (Non-Goals)

- 종료를 막거나 우회하는 것. 이 작업은 관측만 한다.
- Hardlock 응답 material 변경.
- 새 CLI 옵션 추가.

Nothing blocks or bypasses the exit; this task only observes, changes no Hardlock material, and adds no CLI option.
