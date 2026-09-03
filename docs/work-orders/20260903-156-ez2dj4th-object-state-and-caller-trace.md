# Task 156: EZ2DJ 4th null-context 객체 상태·호출자 추적

## 작업 목표

field read 직전 경계 `0x0041a64c`에서 target object `0x00acd708`의 초기화 상태 요약과 호출자 frame chain을 bounded로 수집해, 객체 전체가 미초기화인지 이 field만 비어 있는지 판정하고 초기화 분기 조사를 위한 호출자 경로를 확보합니다.

## 선행 문서

- [Task 156 설계](../design/20260903-156-ez2dj4th-object-state-and-caller-trace.md)
- [Task 155 작업 로그](../work-logs/20260903-155-diagnostic-idle-timeout.md)
- [Task 154 작업 로그](../work-logs/20260903-154-ez2dj4th-field-writer-execution-trace.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. `null_context_object_state.h/.cpp`에 frame chain 수집과 객체 window 스캔을 구현하고 빌드에 추가합니다.
2. `--null-context-object-state-trace` 옵션과 usage 문자열을 추가합니다.
3. `DR0`에 경계 execution breakpoint를 설치하고 새 thread에도 설치합니다.
4. hit, caller frame, 객체 window 요약과 bounded nonzero entry를 JSONL로 기록합니다.
5. hit 상한 도달 시 breakpoint를 해제하고 실행을 계속합니다.
6. 기존 하드웨어 추적 옵션과의 동시 사용을 거부합니다.
7. 설계·작업 로그와 누적 분석 문서를 갱신합니다.

## 비범위

- field 값 직접 주입 또는 patch
- Hardlock 응답 material 변경
- 객체 메모리 전체 덤프
- 초기화 분기 자체의 수정

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 두 번 실행하고 hit·frame·window 요약의 재현성을 확인합니다. 로그에는 원본 자산 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 156: EZ2DJ 4th Null-Context Object State and Caller Trace

## Objective

Collect a bounded initialization summary of target object `0x00acd708` and the caller frame chain at boundary `0x0041a64c`, immediately before the field read, to determine whether the whole object is uninitialized or only this field is empty, and to obtain the caller path needed to investigate the initialization branch.

## Preceding documents

- [Task 156 design](../design/20260903-156-ez2dj4th-object-state-and-caller-trace.md)
- [Task 155 work log](../work-logs/20260903-155-diagnostic-idle-timeout.md)
- [Task 154 work log](../work-logs/20260903-154-ez2dj4th-field-writer-execution-trace.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Implement frame-chain collection and object-window scanning in `null_context_object_state.h/.cpp` and add them to the build.
2. Add the `--null-context-object-state-trace` option and usage text.
3. Install the boundary execution breakpoint in `DR0`, including on newly created threads.
4. Record the hit, caller frames, object-window summary, and bounded nonzero entries as JSONL.
5. Release the breakpoint and continue execution once the hit limit is reached.
6. Reject concurrent use with the existing hardware traces.
7. Update the design, work log, and cumulative analysis document.

## Out of scope

- Direct field injection or patching.
- Changing Hardlock response material.
- Dumping the object's entire memory.
- Modifying the initialization branch itself.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run twice with the extended idle boundary and confirm that the hit, frame, and window summaries reproduce. The logs must not contain original asset contents or Hardlock secret values.
