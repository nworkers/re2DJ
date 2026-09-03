# Task 163: EZ2DJ 4th guard 실패 원인 추적

## 작업 목표

세 guard의 반환값을 실행으로 관찰하고, 실패 코드가 생성되는 지점과 그 직전 연산을 특정해 초기화가 중단되는 직접 원인을 확정합니다.

## 선행 문서

- [Task 163 설계](../design/20260903-163-ez2dj4th-guard-failure-source.md)
- [Task 162 작업 로그](../work-logs/20260903-162-ez2dj4th-slot2-early-exit.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. 진입 추적 대상을 세 guard의 호출 반환 지점과 guard 2 대상 함수 진입으로 바꿉니다.
2. 관찰된 실패 코드를 참조 스캔 값 목록에 추가합니다.
3. 실패 지점과 그 주변을 anchor·body 목록에 추가해 코드 창과 분기 목록을 수집합니다.
4. 실패 함수의 함수 시작과 호출자를 두 단계 분기 추적으로 확인합니다.
5. 설계·작업 로그와 누적 분석 문서를 갱신합니다.

## 비범위

- field 값 직접 주입 또는 patch
- Hardlock 응답 material 변경
- 실패하는 연산의 동작 변경
- 오류 메시지 문자열 내용 기록

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 실행하고, 실패 함수의 호출자가 guard 2 대상 함수 안에 있는지로 체인을 자기 검증합니다. 로그에는 원본 자산 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 163: EZ2DJ 4th Guard Failure Source

## Objective

Observe the three guards' return values at runtime and identify where the failure code is produced and which operation precedes it, establishing the direct cause of the halted initialization.

## Preceding documents

- [Task 163 design](../design/20260903-163-ez2dj4th-guard-failure-source.md)
- [Task 162 work log](../work-logs/20260903-162-ez2dj4th-slot2-early-exit.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Point the entry trace at the three guards' call return points and guard 2's callee entry.
2. Add the observed failure code to the reference scan's value list.
3. Add the failure site and its surroundings to the anchor and body lists to collect code windows and branch listings.
4. Confirm the failing function's start and callers with two-stage branch tracing.
5. Update the design, work log, and cumulative analysis document.

## Out of scope

- Direct field injection or patching.
- Changing Hardlock response material.
- Changing the behavior of the failing operation.
- Recording the error message text.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run with the extended idle boundary and self-check the chain by confirming the failing function's caller lies inside guard 2's callee. The logs must not contain original asset contents or Hardlock secret values.
