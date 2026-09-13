# 작업 로그: EZ2Dancer streaming Play 의미 보정

## 한국어

### 분석

사용자가 제공한 `20260914-001658-331` 로그에서 다음을 확인했습니다.

- `jam.ezw` VFS open/read 성공: `success=1:error=0`
- 초기 read `4096`, `356352`, `4096` bytes 성공
- SDL playback device와 postmix callback 활성화
- JAM streaming track cooked PCM peak 약 `0.74`, RMS 약 `0.52`
- 동일 DirectSound buffer의 Play/streaming-start 6회, 매번 `pos=0` 및 `queued=360448`

이 결과는 파일시스템이나 PCM 로딩 실패가 아니라, HLE가 재생 중 `Play`를 stream
재시작으로 처리하는 문제를 가리켰습니다. Microsoft DirectSound 계약에 맞춰 이미
재생 중인 streaming voice의 일반 `Play`는 control만 갱신하고 queue/cursor를
보존하도록 수정했습니다. 재생 중 `SetCurrentPosition`은 track을 먼저 정지한 뒤
새 위치에서 ring을 다시 공급합니다. trace에는 `streaming-continue`와
`streaming-start`, `continued=1`을 구분해 남깁니다.

### 변경 파일

- `src/audio/sdl3_mixer_audio_backend.h/.cpp`
- `src/platform/windows/directsound_com_facade.cpp`
- `src/tools/windows_vfs_runtime_probe/main.cpp`
- 설계/작업 지시서/분석/KB 색인 및 문서

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- `re2dj_ez2dj_keyboard_input_test` 통과
- `re2dj_ez2dancer_keyboard_input_test` 통과
- `re2dj_windows_product_loader_probe` 통과
- `re2dj_unit_tests` 통과
- `re2dj_windows_vfs_runtime_probe`는 기존 GUI/audio lifecycle 대기 구간에서 완료되지
  않아 중단했습니다. 이번 수정의 반복 Play 검증 코드는 빌드에 포함되었으며, 실제
  JAM 청취 검증은 사용자의 수정 빌드 실행이 필요합니다.

## English

### Analysis

The user-provided `20260914-001658-331` log confirms:

- `jam.ezw` VFS open/read succeeds with `success=1:error=0`.
- Initial reads of `4096`, `356352`, and `4096` bytes succeed.
- The SDL playback device and postmix callback are active.
- JAM streaming-track cooked PCM reaches about `0.74` peak and `0.52` RMS.
- The same DirectSound buffer receives six Play/streaming-start calls, each at `pos=0`
  with `queued=360448`.

This points to HLE handling `Play` while already playing as a stream restart, not a
filesystem or PCM-loading failure. To match the DirectSound contract, ordinary `Play` on an
already-playing streaming voice now updates controls while preserving the queue and cursor.
`SetCurrentPosition` during playback stops the track first and refills it from the new
position. Traces distinguish `streaming-continue` from `streaming-start` and record
`continued=1`.

### Changed files

- `src/audio/sdl3_mixer_audio_backend.h/.cpp`
- `src/platform/windows/directsound_com_facade.cpp`
- `src/tools/windows_vfs_runtime_probe/main.cpp`
- Design, work-order, analysis, KB index, and related documentation

### Verification

- `cmd /c scripts\\build_win32.bat` succeeded.
- `re2dj_ez2dj_keyboard_input_test` passed.
- `re2dj_ez2dancer_keyboard_input_test` passed.
- `re2dj_windows_product_loader_probe` passed.
- `re2dj_unit_tests` passed.
- `re2dj_windows_vfs_runtime_probe` did not complete because it remained in the existing
  GUI/audio lifecycle wait and was interrupted. The repeated-Play regression check is in the
  built probe; real JAM listening validation still requires the user to run the fixed build.
