# 작업 로그: EZ2Dancer SDL 오디오 출력 경계 수정

## 한국어

### 변경 내용

JAM 무음 후보를 확인하기 위해 SDL_mixer streaming track의 exhaustion 정책과 최종
출력 경계를 수정·계측했다.

- `Sdl3MixerAudioBackend::Play()`의 streaming 옵션에
  `MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false`를 추가했다.
- SDL mixer 준비 상태, 실제 playback device 여부, post-mix callback 등록 여부 및
  mixer 출력 포맷을 audio trace에 기록한다.
- post-mix PCM peak/RMS는 처음 16회 callback만 기록하여 오디오 스레드 로그 비용을
  제한했다.
- memory-only mixer fallback은 유지하되 `playback-device=0`으로 확인 가능하게 했다.

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- 관련 CTest 4개 성공:
  `re2dj_ez2dj_keyboard_input_test`, `re2dj_ez2dancer_keyboard_input_test`,
  `re2dj_windows_product_loader_probe`, `re2dj_unit_tests`
- `re2dj_windows_vfs_runtime_probe`는 15초 제한에서 timeout되었다. 이 현상은 기존
  프로브/lifecycle 문제로 관찰되었으며 이번 오디오 변경의 실패로 단정하지 않았다.
- 실제 JAM 실행 로그 검증은 사용자의 새 실행이 필요하다.

### 다음 확인

다음 실행의 `.audio.log`에서 아래 항목을 확인한다.

1. `sdl3:backend`의 `playback-device`가 `1`인지 확인한다.
2. `sdl3:postmix`가 발생하고 peak/RMS가 0보다 큰지 확인한다.
3. `sdl3:play`의 streaming track이 재생 중으로 보고되는지 확인한다.

## English

### Changes

The SDL_mixer streaming exhaustion policy and final output boundary were changed and
instrumented to investigate the JAM silence candidates.

- `Sdl3MixerAudioBackend::Play()` now sets
  `MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false` for streaming playback.
- Audio diagnostics record SDL mixer readiness, physical playback-device availability,
  post-mix callback registration, and mixer output format.
- Post-mix PCM peak/RMS is recorded for only the first 16 callbacks to bound audio-thread
  logging overhead.
- The memory-only mixer fallback remains available but is visible as `playback-device=0`.

### Verification

- `cmd /c scripts\\build_win32.bat` succeeded.
- Four related CTest cases passed:
  `re2dj_ez2dj_keyboard_input_test`, `re2dj_ez2dancer_keyboard_input_test`,
  `re2dj_windows_product_loader_probe`, and `re2dj_unit_tests`.
- `re2dj_windows_vfs_runtime_probe` timed out at 15 seconds. This remains an observed
  probe/lifecycle issue and was not attributed to this audio change.
- A new real JAM run is still required for runtime audio-output confirmation.

### Next check

Inspect the next `.audio.log` for:

1. `playback-device=1` in the `sdl3:backend` record.
2. `sdl3:postmix` records with non-zero peak/RMS.
3. A playing streaming track in the `sdl3:play` record.
