# Task 158: EZ2DJ 4th singleton 전역 참조 스캔

## 작업 목표

전역 pointer `0x00ac29b4`를 참조하는 지점을 모두 세고 참조 형태와 호출 대상을 수집해, 이 singleton을 receiver로 받는 함수 집합과 `+0x11c` 초기화 후보 범위를 확정합니다.

## 선행 문서

- [Task 158 설계](../design/20260903-158-ez2dj4th-singleton-global-scan.md)
- [Task 157 작업 로그](../work-logs/20260903-157-ez2dj4th-object-reference-scan.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. `ImmediateReference`에 직후 바이트 창을 추가하고 leading 창과 같은 폭으로 맞춥니다.
2. `ScanImmediateReferences`가 상한 초과 후에도 계속 세도록 하고 `total_matches`를 돌려줍니다.
3. 단위 테스트를 갱신해 직후 바이트, 버퍼 끝 처리, 상한 초과 총계를 확인합니다.
4. launcher probe의 스캔을 값별 pass로 나누고 `kind` 요약 이벤트를 추가합니다.
5. match 직후가 `call rel32`이면 대상 주소를 계산해 기록합니다.
6. 전역 주소를 스캔 값에 추가하고 그 현재 값과 객체 주소 일치 여부를 기록합니다.
7. 설계·작업 로그와 누적 분석 문서를 갱신합니다.

## 비범위

- field 값 직접 주입 또는 patch
- Hardlock 응답 material 변경
- 완전한 x86 명령 디코더 구현
- 1210개 참조 전체의 수동 디코드

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 실행하고 값별 총계와 callee 집합을 확인합니다. 로그에는 원본 자산 내용이나 Hardlock secret 값을 기록하지 않습니다.

---

# Task 158: EZ2DJ 4th Singleton Global Reference Scan

## Objective

Count every reference to the global pointer `0x00ac29b4` and collect the reference forms and call targets, establishing the set of functions that take this singleton as a receiver and bounding where `+0x11c` initialization can live.

## Preceding documents

- [Task 158 design](../design/20260903-158-ez2dj4th-singleton-global-scan.md)
- [Task 157 work log](../work-logs/20260903-157-ez2dj4th-object-reference-scan.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. Add a trailing-byte window to `ImmediateReference`, matching the leading window's width.
2. Make `ScanImmediateReferences` keep counting past the cap and return `total_matches`.
3. Update the unit tests for trailing bytes, buffer-end handling, and totals past the cap.
4. Split the launcher probe's scan into one pass per value and add the `kind` summary event.
5. Resolve and record the target when a match is directly followed by `call rel32`.
6. Add the global address to the scanned values and record whether its current value equals the object address.
7. Update the design, work log, and cumulative analysis document.

## Out of scope

- Direct field injection or patching.
- Changing Hardlock response material.
- Implementing a complete x86 instruction decoder.
- Manually decoding all 1210 references.

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run with the extended idle boundary and check the per-value totals and callee set. The logs must not contain original asset contents or Hardlock secret values.
