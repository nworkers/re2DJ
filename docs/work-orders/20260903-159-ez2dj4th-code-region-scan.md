# Task 159: EZ2DJ 4th 코드 영역 스캔

## 작업 목표

field read anchor, 실행되는 두 `+0x11c` write 후보, vtable 설치 두 지점의 함수 시작과 코드 영역을 수집해, field 읽기 전 검사 유무와 write 후보의 대상 객체를 판정합니다.

## 선행 문서

- [Task 159 설계](../design/20260903-159-ez2dj4th-code-region-scan.md)
- [Task 158 작업 로그](../work-logs/20260903-158-ez2dj4th-singleton-global-scan.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. 공용 코어에 `re2dj::exe::FindPrologueBefore`와 헤더를 추가하고 빌드에 연결합니다.
2. 해당 함수의 단위 테스트를 추가하고 test 목록에 등록합니다.
3. 참조 스캔 뒤 anchor 목록에 대해 함수 시작을 찾고 코드 영역을 JSONL로 기록합니다.
4. anchor가 함수 시작 창 밖이면 anchor 중심 창을 추가로 기록합니다.
5. 설계·작업 로그와 누적 분석 문서를 갱신합니다.

## 비범위

- field 값 직접 주입 또는 patch
- Hardlock 응답 material 변경
- 완전한 x86 명령 디코더 구현
- 추가 원격 메모리 읽기

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 실행하고 각 anchor의 함수 시작과 창을 확인합니다. 로그에는 원본 자산 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 159: EZ2DJ 4th Code Region Scan

## Objective

Collect the function starts and code regions of the field-read anchor, the two executing `+0x11c` write candidates, and the two vtable installation sites, to determine whether the field is checked before the read and which object the write candidates target.

## Preceding documents

- [Task 159 design](../design/20260903-159-ez2dj4th-code-region-scan.md)
- [Task 158 work log](../work-logs/20260903-158-ez2dj4th-singleton-global-scan.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Add `re2dj::exe::FindPrologueBefore` and its header to the shared core and wire them into the build.
2. Add unit tests for that function and register them in the test list.
3. After the reference scan, locate the function start for each anchor and record the code region as JSONL.
4. Record an anchor-centered window as well when the anchor falls outside the function-start window.
5. Update the design, work log, and cumulative analysis document.

## Out of scope

- Direct field injection or patching.
- Changing Hardlock response material.
- Implementing a complete x86 instruction decoder.
- Additional remote memory reads.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run with the extended idle boundary and check each anchor's function start and windows. The logs must not contain original asset contents or Hardlock secret values.
