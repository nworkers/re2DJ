# Task 160: EZ2DJ 4th field initializer 호출 체인

## 작업 목표

실행되지 않은 write 후보 `0x0001825f`·`0x0001dbd3`의 함수와 write receiver를 확인하고, 상대 분기 추적으로 호출 체인을 따라가 singleton의 `+0x11c`를 채우는 유일한 경로를 확정합니다.

## 선행 문서

- [Task 160 설계](../design/20260903-160-ez2dj4th-field-initializer-chain.md)
- [Task 159 작업 로그](../work-logs/20260903-159-ez2dj4th-code-region-scan.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. 공용 코어 `code_scan`에 `ScanRelativeBranches`를 추가합니다.
2. 해당 함수의 단위 테스트를 추가합니다.
3. anchor 목록에 후보 2·3과 후보 2의 호출 지점을 추가합니다.
4. 함수 시작과 thunk 두 단계의 분기 추적 결과를 JSONL로 기록합니다.
5. prologue 역방향 검색 범위를 넓힙니다.
6. 설계·작업 로그와 누적 분석 문서를 갱신합니다.

## 비범위

- field 값 직접 주입 또는 patch
- Hardlock 응답 material 변경
- 완전한 x86 명령 디코더 구현
- 초기화 경로를 강제로 실행시키는 변경

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 실행하고, field read 함수가 알려진 thunk 하나만 가리키는지로 추적 방법을 자기 검증한 뒤 체인을 확인합니다. 로그에는 원본 자산 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 160: EZ2DJ 4th Field-Initializer Call Chain

## Objective

Determine the functions and write receivers of the non-executing write candidates `0x0001825f` and `0x0001dbd3`, and follow the call chain with relative-branch tracing to establish the one route that fills the singleton's `+0x11c`.

## Preceding documents

- [Task 160 design](../design/20260903-160-ez2dj4th-field-initializer-chain.md)
- [Task 159 work log](../work-logs/20260903-159-ez2dj4th-code-region-scan.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Add `ScanRelativeBranches` to the shared-core `code_scan`.
2. Add unit tests for that function.
3. Add candidates 2 and 3 and candidate 2's call site to the anchor list.
4. Record the two-stage branch tracing results, for the function start and for each thunk, as JSONL.
5. Widen the backward prologue search range.
6. Update the design, work log, and cumulative analysis document.

## Out of scope

- Direct field injection or patching.
- Changing Hardlock response material.
- Implementing a complete x86 instruction decoder.
- Any change that forces the initialization path to run.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run with the extended idle boundary, self-check the tracing method by confirming the field-read function resolves to its one known thunk, then check the chain. The logs must not contain original asset contents or Hardlock secret values.
