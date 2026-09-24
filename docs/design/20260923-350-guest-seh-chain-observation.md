# 작업 350: 게스트 SEH 체인 관찰 및 INT3 예외 전달 설계 / Task 350: Guest SEH chain observation and INT3 exception dispatch design

선행: [작업 349 설계](20260923-349-linux-inprocess-continuation.md), [작업 349 작업 로그](../work-logs/20260923-349-linux-inprocess-continuation.md), [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)

## 배경 / Background

작업 349의 `--linux-in-process-continue` 연속 실행은 13개 API 호출 후 게스트 자체 코드 `0x00af1135`에서 `INT3`(`0xCC`)를 실행해 SIGTRAP으로 멈췄습니다. 그 시점의 레지스터는 ESI=`'FG'`, EDI=`'JM'`, ECX=`0x00004450` 상태였습니다. 이는 SoftICE 존재 여부를 감지하기 위한 표준적인 안티 디버깅 패턴과 일치합니다.

이 패턴에서 정상적인 Win32 환경이라면:
1. 게스트가 `INT3`를 실행하기 전에 `FS:[0]`에 자체 SEH(Structured Exception Handling) 프레임을 등록합니다.
2. SoftICE 디버거가 없으면 CPU는 `#BP` 예외(Interrupt 3)를 발생시키고, OS는 이를 Win32 `EXCEPTION_BREAKPOINT`(`0x80000003`) 예외로 변환하여 스레드의 SEH 체인(`FS:[0]`)에 전달합니다.
3. 게스트의 SEH 핸들러가 호출되어 예외를 처리하고 `ContextRecord->Eip`를 `INT3` 다음 명령으로 조정한 뒤 `ExceptionContinueExecution`(`0`)을 반환합니다.

현재 re2DJ의 Linux i386 실행 환경(`NativeProcessBootstrap`)은 `FS` 세그먼트 디스크립터와 TEB/PEB를 할당하고 `FS:[0]`을 초기값 `0xFFFFFFFF`로 설정했으나, 게스트가 실제로 SEH 체인을 갱신했는지 관찰하지 않으며, SIGTRAP 발생 시 `siglongjmp`를 통해 즉시 정지합니다.

*Task 349's `--linux-in-process-continue` continuation run stopped with SIGTRAP after 13 API calls at the guest's own `INT3` (`0xCC`) at `0x00af1135`. Registers at that point were ESI=`'FG'`, EDI=`'JM'`, ECX=`0x00004450`. This matches the standard anti-debugging idiom used to detect SoftICE.*

*In a normal Win32 environment:*
*1. The guest installs its own SEH (Structured Exception Handling) frame into `FS:[0]` prior to executing `INT3`.*
*2. Without SoftICE, the CPU generates a `#BP` exception (Interrupt 3), and the OS translates it into a Win32 `EXCEPTION_BREAKPOINT` (`0x80000003`) dispatched to the thread's SEH chain (`FS:[0]`).*
*3. The guest's SEH handler runs, handles the exception, advances `ContextRecord->Eip` past the `INT3`, and returns `ExceptionContinueExecution` (`0`).*

*Currently re2DJ's Linux i386 runner (`NativeProcessBootstrap`) sets up the `FS` segment and TEB/PEB with `FS:[0]` initially `0xFFFFFFFF`, but does not observe subsequent SEH chain updates by the guest, and immediately halts on SIGTRAP via `siglongjmp`.*

```mermaid
sequenceDiagram
    participant Guest as Guest Code
    participant TEB as TEB (FS:[0])
    participant Host as Linux Signal Handler
    participant Dispatcher as HLE SEH Dispatcher
    participant Handler as Guest SEH Handler

    Guest->>TEB: push handler; push prev; mov fs:[0], esp
    Guest->>Guest: mov esi, 'FG'; mov edi, 'JM'; int 3
    Guest-->>Host: SIGTRAP (#BP)
    Host->>Host: Check if fault is INT3 & SEH registered
    alt SEH registered at FS:[0]
        Host->>Dispatcher: Build EXCEPTION_RECORD & CONTEXT on guest stack
        Dispatcher->>Handler: Call handler(pRecord, pFrame, pContext, pDispatcher)
        Handler-->>Dispatcher: Return ExceptionContinueExecution (0)
        Dispatcher->>Guest: Resume execution from updated Context.Eip
    else No handler or unhandled
        Host->>Host: Halt and report fault observation
    end
```

---

## 1단계 결정: 게스트 SEH 상태 관찰 / Step 1 Decision: Guest SEH State Observation

디스패치를 무턱대고 수행하기 전에, **게스트가 실제로 `FS:[0]`에 SEH 프레임을 등록했는지 관찰**해야 합니다. 확인되지 않은 가정을 전제로 코드를 복잡하게 만들기보다 관측 결과를 바탕으로 확정적 판단을 내립니다.

1. **`NativeProcessBootstrap`에 TEB 접근자 추가**:
   - `teb()`: TEB 가상 주소 반환.
   - `IsGuestStackRange(address, size)`: 주소가 유효한 게스트 스택 범위인지 검증.
2. **`NativeFaultObservation` / `OriginalFaultObservation` 확장**:
   - `fs_base`: 게스트 TEB 주소.
   - `seh_frame_address`: `FS:[0]`에 저장된 현재 SEH 프레임 헤드 포인터 (`Tib.ExceptionList`).
   - `seh_next`: 프레임의 `Next` 포인터.
   - `seh_handler`: 프레임의 예외 핸들러 함수 포인터 (`PEXCEPTION_ROUTINE`).
   - `seh_frame_observed`: 유효한 스택 상의 SEH 프레임을 정상적으로 읽었는지 여부.
3. **CLI 진단 보고 확장**:
   - `--linux-in-process-continue` 등의 fault 보고 시 SEH 체인 상태(`fs_base`, `FS:[0]`, `handler`, `next`)를 함께 출력.

*Before attempting dispatch, we must **observe whether the guest actually registered an SEH frame in `FS:[0]`**. Rather than adding complex code on unconfirmed assumptions, we base our next steps on verified observations.*

*1. Add TEB accessors to `NativeProcessBootstrap` (`teb()`, stack range validation).*
*2. Extend `NativeFaultObservation` and `OriginalFaultObservation` with `fs_base`, `seh_frame_address`, `seh_next`, `seh_handler`, and `seh_frame_observed`.*
*3. Extend CLI diagnostic reports to display SEH chain state on fault.*

---

## 2단계 결정: 게스트 INT3 예외 디스패치 / Step 2 Decision: Guest INT3 Exception Dispatch

관찰 결과 `FS:[0]`에 유효한 핸들러가 확인되는 경우, `NativeProcessBootstrap` 또는 runner에서 Win32 SEH dispatch를 수행합니다.

### Win32 x86 SEH 프레임 및 구조체 규약

```c
struct EXCEPTION_REGISTRATION_RECORD {
    struct EXCEPTION_REGISTRATION_RECORD* Next; // 0xFFFFFFFF at end
    PEXCEPTION_ROUTINE Handler;
};

// Handler signature (__cdecl)
EXCEPTION_DISPOSITION __cdecl ExceptionHandler(
    PEXCEPTION_RECORD ExceptionRecord,
    PVOID EstablisherFrame,
    PCONTEXT ContextRecord,
    PVOID DispatcherContext
);
```

- **`EXCEPTION_RECORD`**:
  - `ExceptionCode = 0x80000003` (`EXCEPTION_BREAKPOINT`)
  - `ExceptionFlags = 0` (continuable)
  - `ExceptionRecord = nullptr`
  - `ExceptionAddress = faulting_instruction_pointer` (즉 `0x00af1135`)
  - `NumberParameters = 0`
- **`CONTEXT`**:
  - `ContextFlags = CONTEXT_FULL` (`0x00010007`)
  - 레지스터: `Eip`, `Esp`, `Ebp`, `Eax`, `Ebx`, `Ecx`, `Edx`, `Esi`, `Edi`, `EFlags`, `SegFs` 등.
  - 주의: x86 Linux에서 `ucontext_t`의 `uc_mcontext.gregs[REG_EIP]`는 `INT3` 실행 후이므로 `0x00af1136`를 가리킵니다. Win32 `ExceptionAddress`는 `0x00af1135`이며, `Context.Eip`는 통상 예외 발생 주소(`0x00af1135`) 또는 핸들러가 기대하는 복귀 기준에 맞게 설정됩니다.
- **반환 처리**:
  - 핸들러 반환값 `EXCEPTION_DISPOSITION`:
    - `0` (`ExceptionContinueExecution`): 핸들러가 조정한 `Context`로 게스트 실행 재개.
    - `1` (`ExceptionContinueSearch`): 다음 SEH 프레임(`Next`) 탐색.
    - 기타: 처리 불능 fault로 중단.

*When an SEH handler is confirmed in `FS:[0]`, the runner dispatches the Win32 `EXCEPTION_BREAKPOINT` to the registered handler using the Win32 x86 SEH calling convention. If the handler returns `ExceptionContinueExecution` (0), execution resumes at the updated context EIP.*

---

## 확인됨 / 추정 / 미확정 / Confirmed, Inferred, Unresolved

* **확인됨**: 작업 349에서 원본이 `0x00af1135`의 `INT3`를 실행했고, 당시 ESI=`'FG'`, EDI=`'JM'`이었다.
* **추정**: 이는 SoftICE 감지 루틴이며, 직전에 `FS:[0]`에 SEH 핸들러를 등록했을 것이다.
* **미확정**: `0x00af1135` 시점에서 `FS:[0]`의 실제 값과 핸들러 함수 주소, 핸들러 내부 구현은 아직 관찰되지 않았다. 1단계 관찰을 통해 확인한다.

* *Confirmed: In Task 349, original code executed `INT3` at `0x00af1135` with ESI=`'FG'`, EDI=`'JM'`.*
* *Inferred: This is a SoftICE detection routine that registered an SEH handler in `FS:[0]` beforehand.*
* *Unresolved: The actual value of `FS:[0]`, handler address, and handler behavior at `0x00af1135` are not yet observed. Step 1 will confirm them.*
