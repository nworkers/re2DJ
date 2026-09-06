# EZ2DJ 6th bootstrap 자식 프로세스 추적 작업 로그

## 결과

`ez2dj6th` 프로파일이 CHD 내부의 `EZ2DJ/EZ2DJ.EXE` bootstrap을 실행하고,
bootstrap이 생성하는 `EZ2DJ6th.EXE` child를 `DEBUG_PROCESS` 이벤트로 추적하도록
변경했습니다. bootstrap에는 child 생성에 필요한 VFS/device/Hardlock 경계를 유지하고,
실제 game child의 entry에서 runtime과 graphics/audio/input HLE를 준비합니다.

프로파일의 `executable_relative_path`는 `EZ2DJ/EZ2DJ.EXE`로 바뀌었고,
`follow_child_process` 기본값이 추가되었습니다. 기존 다른 프로파일의 단일 프로세스
경로는 유지합니다.

## 구현

- `child_process_handoff.*`를 추가해 child PE 분석, software entry breakpoint,
  injected runtime 로드, VFS/device/Hardlock/DirectX import 연결을 분리했습니다.
- launcher에 `--follow-child`를 추가하고 해당 모드에서만 `DEBUG_PROCESS`를 사용합니다.
- child 생성 path를 확인한 뒤 primary thread가 suspended인 상태에서 runtime을 주입하고,
  child 종료 debug event까지 기다립니다.
- profile/backend 경계에 `follow_child_process`를 추가하고 6th profile에 연결했습니다.
- 6th profile 테스트 기대값과 CHD/bootstrap 분석 문서를 갱신했습니다.

## 검증

- Windows x86 Debug build: 성공
- `re2dj_unit_tests.exe`: `checks: 1374, failures: 0`
- 명시적 synthetic baseline 진단 실행: 성공
  - `child_process_created`: `EZ2DJ6th.EXE`
  - `child_runtime_prepared`: 기록됨
  - `child_process_boundary`: `exit`, code `0x00000000`
  - `preparation_status`: `handoff_observed=true`, `iat_verified=true`

위 synthetic baseline은 process-follow plumbing 검증에만 사용했으며, 6th의 실제
Hardlock response/seed 또는 최종 게임 실행 성공으로 승격하지 않았습니다. 기본 profile
실행은 아직 유효한 6th Hardlock material이 없으면 bootstrap의 device request에서
대기합니다.

---

# EZ2DJ 6th Bootstrap Child-Process Follow Work Log

## Result

The `ez2dj6th` profile now launches `EZ2DJ/EZ2DJ.EXE` from the CHD and follows the
`EZ2DJ6th.EXE` child through `DEBUG_PROCESS` events. The bootstrap keeps the
VFS/device/Hardlock boundaries required to create the child, while the actual game
child receives runtime and graphics/audio/input HLE at its entry point.

The profile executable path is now `EZ2DJ/EZ2DJ.EXE`, with the new
`follow_child_process` default. Other profiles retain their existing single-process
paths.

## Implementation

- Added `child_process_handoff.*` for child PE inspection, software entry breakpoint,
  injected-runtime loading, and VFS/device/Hardlock/DirectX import wiring.
- Added `--follow-child`; only this mode uses `DEBUG_PROCESS`.
- The launcher validates the child image, injects while its primary thread is suspended,
  and waits through the child exit debug event.
- Added `follow_child_process` at the profile/backend boundary and enabled it for 6th.
- Updated profile tests and CHD/bootstrap analysis documents.

## Verification

- Windows x86 Debug build: passed
- `re2dj_unit_tests.exe`: `checks: 1374, failures: 0`
- Explicit synthetic-baseline diagnostic run: passed
  - `child_process_created`: `EZ2DJ6th.EXE`
  - `child_runtime_prepared`: recorded
  - `child_process_boundary`: `exit`, code `0x00000000`
  - `preparation_status`: `handoff_observed=true`, `iat_verified=true`

The synthetic baseline was used only to verify process-follow plumbing. It was not
promoted as the real 6th Hardlock response/seed or as final game success. A normal
profile run still waits at the bootstrap device request until valid 6th Hardlock
material is available.
