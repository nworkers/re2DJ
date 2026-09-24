# 작업 지시 344: 게스트 모듈 resolver 합류 / Work order 344: Guest module resolver convergence

## 목표 / Objective

[작업 344 설계](../design/20260922-344-guest-module-resolver-convergence.md)에 따라 Linux i386 진단의 pseudo `kernel32` resolver를 실제 PE32 facade와 registry 기반 경로로 교체하고, 실제 4th CHD의 `\\.\NTICE` 호출까지 회귀 검증합니다.

*Replace the Linux i386 diagnostic's pseudo-`kernel32` resolver with the real PE32 facade and registry-backed path described in the [Task 344 design](../design/20260922-344-guest-module-resolver-convergence.md), then regress through the real 4th CHD's `\\.\NTICE` call.*

## 작업 / Work

1. bounded guest 문자열 및 module/export 조회를 제공하는 선택적 import-call service를 추가하고 `kernel32` resolver handler를 연결합니다.
2. Linux i386 session에서 `kernel32` facade를 main image와 충돌 없이 mapping하고 등록된 정적 IAT를 facade thunk로 재결합합니다.
3. `native_create_file_observation.cpp`에서 `0x7F000001`, 별도 dynamic thunk, API 이름별 resolver 분기를 제거합니다.
4. 작업 343의 i386 전용 facade source와 probe를 `src/platform/linux/x86/`으로 옮기고 CMake 경로를 갱신합니다.
5. synthetic identity 검증과 실제 `roms/ez2dj4th/ez2dj4th.chd` 실행으로 facade base, export 주소, `CreateFileA` 인자와 stack cleanup을 확인합니다.
6. 관련 architecture, analysis, TODO와 작업 로그를 갱신하고 영향받는 Windows x86/x64 및 Linux x86/x64 build/test를 수행합니다.

*Add optional import-call services for bounded guest strings and registry resolution; map and register `kernel32` during Linux i386 session preparation; rebind registered static IAT entries to facade thunks; remove the pseudo handle, separate dynamic thunks, and per-API resolver branches; move Task 343's i386-only sources under `linux/x86/`; validate synthetic identity and the real 4th CHD; and update the related architecture, analysis, TODO, work log, builds, and tests.*

## 완료 조건 / Completion criteria

- `CreateFileA` 진단 경로에 `0x7F000001`과 진단 전용 `GetVersion`/`CreateFileA` dynamic thunk가 남지 않습니다.
- `kernel32` handle은 mapped facade base이며 정적 IAT와 동적 resolver가 같은 registered export thunk를 반환합니다.
- 실제 4th CHD가 `CreateFileA("\\\\.\\NTICE")` 호출과 28-byte `__stdcall` cleanup 뒤의 제한된 복귀 경계에 도달합니다.
- 원본 CHD는 읽기 전용으로 사용되며 추출 PE나 생성 facade binary를 저장소에 추가하지 않습니다.
- 검증 결과와 미지원 범위를 작업 로그에 기록하고 변경을 하나의 Git 커밋으로 남깁니다.

*No pseudo handle or diagnostic-only dynamic thunk remains; the facade base and registered export thunks provide shared static/dynamic identity; the real 4th CHD reaches bounded return after `CreateFileA("\\\\.\\NTICE")` with 28-byte `__stdcall` cleanup; original assets remain read-only and uncommitted; and validation plus remaining scope are recorded in one task commit.*
