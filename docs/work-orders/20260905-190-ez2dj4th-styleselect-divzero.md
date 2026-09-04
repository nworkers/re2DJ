# 작업 지시서: EZ2DJ 4th Trax StyleSelect 0xC0000094 예외 진단 및 해결

## 1. 작업 개요 (Task Overview)

- **작업 ID**: 190
- **작업명**: `ez2dj4th-styleselect-divzero`
- **대상**: EZ2DJ 4th Trax (`EZ2DJ/EZ2DJ.EXE`)
- **목표**: ModeSelect(StreetMix) 이후 `STYLE_SELECT.str` 로드 시점에 발생하는 `0xC0000094` (`STATUS_INTEGER_DIVIDE_BY_ZERO`) 예외의 원인을 규명하고, HLE 경계를 수정하여 StyleSelect 및 곡 선택으로 정상 진입하도록 한다.
- **관련 설계 문서**: `docs/design/20260905-190-ez2dj4th-styleselect-divzero.md`

- **Task ID**: 190
- **Task Name**: `ez2dj4th-styleselect-divzero`
- **Target**: EZ2DJ 4th Trax (`EZ2DJ/EZ2DJ.EXE`)
- **Objective**: Identify the root cause of the `0xC0000094` (`STATUS_INTEGER_DIVIDE_BY_ZERO`) exception that occurs upon loading `STYLE_SELECT.str` after ModeSelect (StreetMix), and update the HLE boundary so that the game successfully enters StyleSelect and song selection.
- **Related Design**: `docs/design/20260905-190-ez2dj4th-styleselect-divzero.md`

---

## 2. 작업 단계 (Action Items)

### 1단계: VEH 포렌식 로거 확장 (Phase 1: Extend VEH Forensic Logger)
- `src/platform/windows/injected_runtime.cpp`의 Vectored Exception Handler에 비정상 종료 예외(`EXCEPTION_INT_DIVIDE_BY_ZERO`, `EXCEPTION_ACCESS_VIOLATION` 등)를 인터셉트하여 EIP, RVA, 범용 레지스터, 명령어 바이트, 스택 상위 워드를 `vfs.log`에 기록하는 `ReportCrashException` 함수 구현.
- 빌드 수행 및 `re2dj_injected_runtime.dll` 생성 확인.

### 2단계: 크래시 재현 및 EIP 분석 (Phase 2: Reproduce Crash & Pinpoint EIP)
- EZ2DJ 4th Trax를 실행하고 StreetMix 진입 후 `STYLE_SELECT.str` 로드 시점의 크래시 유도.
- `vfs.log`의 `re2dj:vfs:crash-exception` 라인을 확인하여 EIP 주소, RVA, 제수 레지스터, 디스어셈블리 코드 확인.

### 3단계: 근본 원인 분석 및 HLE 수정 (Phase 3: Root-Cause Fix)
- 제수가 0이 된 데이터 소스(VFS 검색, DirectDraw 모드 정보, 타이머, 오디오 등)를 규명하고 알맞은 HLE 방어/에뮬레이션 로직 구현.
- 단위 테스트 실행 및 게임 런타임 재실행으로 정상 동작 확인.

### 4단계: 문서화 및 커밋 (Phase 4: Documentation & Commit)
- 작업 로그 `docs/work-logs/20260905-190-ez2dj4th-styleselect-divzero.md` 작성.
- 관련 설계 및 아키텍처 문서 갱신.
- Git 커밋 생성.

---

## 3. 검증 기준 (Verification Criteria)

1. `ninja -C build/windows-x86` 빌드가 경고 및 에러 없이 성공할 것.
2. `re2dj_unit_tests.exe`가 모두 통과할 것.
3. EZ2DJ 4th Trax 실행 시 StreetMix 선택 후 `0xC0000094` 예외 없이 StyleSelect 화면으로 진입할 것.
