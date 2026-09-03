# Task 157: EZ2DJ 4th null-context 객체 참조 스캔

## 작업 목표

복호화된 런타임 `.text`에서 target object `0x00acd708`과 그 vtable을 immediate로 참조하는 지점을 수집하고, vtable slot과 호출자 코드 창을 함께 기록해 객체의 클래스와 접근 경로를 식별합니다.

## 선행 문서

- [Task 157 설계](../design/20260903-157-ez2dj4th-object-reference-scan.md)
- [Task 156 작업 로그](../work-logs/20260903-156-ez2dj4th-object-state-and-caller-trace.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. 공용 코어에 `re2dj::exe::ScanImmediateReferences`와 헤더를 추가하고 빌드에 연결합니다.
2. 해당 함수의 단위 테스트를 추가하고 test 목록에 등록합니다.
3. launcher probe에 `--null-context-object-reference-scan` 옵션과 usage 문자열을 추가합니다.
4. 경계 breakpoint 설치, 단발 수집, breakpoint 해제 경로를 추가합니다.
5. vtable slot, immediate match, caller 코드 창, 스캔 요약을 JSONL로 기록합니다.
6. 기존 하드웨어 추적 옵션과의 충돌을 거부하고 `ez2dj4th` 외 target을 거부합니다.
7. 설계·작업 로그와 누적 분석 문서를 갱신합니다.

## 비범위

- field 값 직접 주입 또는 patch
- Hardlock 응답 material 변경
- 완전한 x86 명령 디코더 구현
- 초기화 분기 자체의 수정

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 두 번 실행하고 match 집합의 재현성을 확인합니다. 로그에는 원본 자산 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 157: EZ2DJ 4th Null-Context Object Reference Scan

## Objective

Collect the sites in the decrypted runtime `.text` that reference target object `0x00acd708` and its vtable as immediates, and record the vtable slots and caller code windows, to identify the object's class and access path.

## Preceding documents

- [Task 157 design](../design/20260903-157-ez2dj4th-object-reference-scan.md)
- [Task 156 work log](../work-logs/20260903-156-ez2dj4th-object-state-and-caller-trace.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Add `re2dj::exe::ScanImmediateReferences` and its header to the shared core and wire them into the build.
2. Add unit tests for that function and register them in the test list.
3. Add the `--null-context-object-reference-scan` option and usage text to the launcher probe.
4. Add the boundary-breakpoint installation, single-shot collection, and breakpoint-release path.
5. Record vtable slots, immediate matches, caller code windows, and the scan summary as JSONL.
6. Reject conflicts with the existing hardware traces and refuse targets other than `ez2dj4th`.
7. Update the design, work log, and cumulative analysis document.

## Out of scope

- Direct field injection or patching.
- Changing Hardlock response material.
- Implementing a complete x86 instruction decoder.
- Modifying the initialization branch itself.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run twice with the extended idle boundary and confirm the match set reproduces. The logs must not contain original asset contents or Hardlock secret values.
