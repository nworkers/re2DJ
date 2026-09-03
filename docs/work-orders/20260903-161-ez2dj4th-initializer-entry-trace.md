# Task 161: EZ2DJ 4th initializer 진입 추적

## 작업 목표

Task 160이 정적으로 확정한 초기화 체인의 네 지점에 실행 breakpoint를 걸어, 각 지점의 진입 여부·receiver·호출자를 실행 증거로 기록하고 체인이 끊기는 위치를 판정합니다.

## 선행 문서

- [Task 161 설계](../design/20260903-161-ez2dj4th-initializer-entry-trace.md)
- [Task 160 작업 로그](../work-logs/20260903-160-ez2dj4th-field-initializer-chain.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. `--null-context-entry-trace` 옵션과 usage 문자열을 추가합니다.
2. 네 진입 주소를 `DR0`–`DR3`에 설치하는 helper와 새 thread 설치 경로를 추가합니다.
3. hit에서 receiver, `ESP`, 호출자 반환 주소와 RVA를 JSONL로 기록합니다.
4. hit 후 `TF` 단일-step 통과와 breakpoint 복구 경로를 추가합니다.
5. 진입별 기록 상한과 boundary 요약을 연결합니다.
6. 기존 하드웨어 추적 옵션과의 충돌 및 target 제한을 거부 조건으로 추가합니다.
7. 설계·작업 로그와 누적 분석 문서를 갱신합니다.

## 비범위

- field 값 직접 주입 또는 patch
- Hardlock 응답 material 변경
- 초기화 경로를 강제로 실행시키는 변경
- 함수 내부 전체 명령 추적

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 두 번 실행하고 hit 집합의 재현성과 종료 코드를 확인합니다. 로그에는 원본 자산 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 161: EZ2DJ 4th Initializer Entry Trace

## Objective

Set execution breakpoints on the four points of the initialization chain Task 160 established statically, record each point's entry, receiver, and caller as execution evidence, and determine where the chain breaks.

## Preceding documents

- [Task 161 design](../design/20260903-161-ez2dj4th-initializer-entry-trace.md)
- [Task 160 work log](../work-logs/20260903-160-ez2dj4th-field-initializer-chain.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Add the `--null-context-entry-trace` option and usage text.
2. Add the helper that installs the four entry addresses in `DR0`–`DR3` and the new-thread installation path.
3. Record the receiver, `ESP`, and the caller's return address and RVA on each hit as JSONL.
4. Add the `TF` single-step pass and breakpoint restoration after a hit.
5. Connect per-entry record limits and boundary summaries.
6. Reject conflicts with the existing hardware traces and non-`ez2dj4th` targets.
7. Update the design, work log, and cumulative analysis document.

## Out of scope

- Direct field injection or patching.
- Changing Hardlock response material.
- Any change that forces the initialization path to run.
- Tracing every instruction inside the functions.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run twice with the extended idle boundary and confirm the hit set and exit code reproduce. The logs must not contain original asset contents or Hardlock secret values.
