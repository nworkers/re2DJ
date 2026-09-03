# Task 162: EZ2DJ 4th slot 2 메서드 조기 이탈 분석

## 작업 목표

vtable slot 2 메서드 본문의 분기를 구조화해 초기화 호출을 건너뛰는 이탈 지점을 찾고, 실행 breakpoint로 실제 선택되는 경로와 직전 호출의 반환값을 측정합니다.

## 선행 문서

- [Task 162 설계](../design/20260903-162-ez2dj4th-slot2-early-exit.md)
- [Task 161 작업 로그](../work-logs/20260903-161-ez2dj4th-initializer-entry-trace.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. 공용 코어 `code_scan`에 `ListNearBranches`를 추가하고 단위 테스트를 작성합니다.
2. launcher probe가 지정 함수 범위의 분기 목록과 `skips_call` 표시를 JSONL로 기록합니다.
3. 진입 추적의 대상 주소를 이탈 후보로 바꾸고 `EAX`·`EDX`를 기록에 추가합니다.
4. 이탈 지점과 관련 thunk를 anchor·body 목록에 추가해 코드와 대상 주소를 해석합니다.
5. 설계·작업 로그와 누적 분석 문서를 갱신합니다.

## 비범위

- field 값 직접 주입 또는 patch
- Hardlock 응답 material 변경
- 완전한 x86 명령 디코더 구현
- 실패하는 호출의 내부 동작 수정

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 실행하고, 분기 목록에 초기화 호출 지점이 `call`로 나타나는지로 자기 검증한 뒤 실행된 이탈 지점을 확인합니다. 로그에는 원본 자산 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 162: EZ2DJ 4th Slot 2 Method Early-Exit Analysis

## Objective

Structure the branches of the vtable slot 2 method body to find the exits that skip the initializer call, and measure with execution breakpoints which path is actually taken and what the preceding call returned.

## Preceding documents

- [Task 162 design](../design/20260903-162-ez2dj4th-slot2-early-exit.md)
- [Task 161 work log](../work-logs/20260903-161-ez2dj4th-initializer-entry-trace.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Add `ListNearBranches` to the shared-core `code_scan` with unit tests.
2. Have the launcher probe record the branch listing for a given function range with the `skips_call` marking as JSONL.
3. Point the entry trace at the exit candidates and add `EAX` and `EDX` to its records.
4. Add the exit sites and related thunks to the anchor and body lists to interpret the code and resolve targets.
5. Update the design, work log, and cumulative analysis document.

## Out of scope

- Direct field injection or patching.
- Changing Hardlock response material.
- Implementing a complete x86 instruction decoder.
- Changing the behavior of the failing call.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run with the extended idle boundary, self-check that the initializer call site appears as a `call` in the listing, then confirm the exit taken. The logs must not contain original asset contents or Hardlock secret values.
