# 작업 로그: EZ2Dancer 스트리밍 트랙 재시작 수명 수정

## 한국어

### 결과

`jam.ezw`가 CHD VFS에서 정상적으로 읽히지만 `Stop` 후 동일 SDL_mixer track을 재사용할
때 이전 output stream이 남을 수 있는 문제를 수정했습니다. stopped streaming voice의
명시적 새 `Play`마다 `MIX_Track`을 새로 만들고, 기존 input `SDL_AudioStream`과 cooked
callback을 다시 연결합니다. 이미 재생 중인 반복 `Play`의 queue/cursor 보존 동작은
변경하지 않았습니다.

### 변경 파일

* `src/audio/sdl3_mixer_audio_backend.h/.cpp`
  * `ResetTrack` helper 추가
  * Stop 이후 streaming 재생에서 track 재생성
  * `track-reset` 진단 marker 추가
* `src/tools/windows_vfs_runtime_probe/main.cpp`
  * Stop 후 track reset marker 회귀 검사 추가
* `docs/design/20260914-282-ez2d2m-streaming-track-restart.md`
* `docs/work-orders/20260914-282-ez2d2m-streaming-track-restart.md`
* `docs/analysis/ez2d2m-jam-audio-runtime.md`

### 검증

* `cmd /c scripts\build_win32.bat` 성공
* `re2dj_ez2dj_keyboard_input_test` 통과
* `re2dj_ez2dancer_keyboard_input_test` 통과
* `re2dj_windows_product_loader_probe` 통과
* `re2dj_unit_tests` 통과
* `re2dj_windows_vfs_runtime_probe.exe --audio-exit-child` 정상 종료
* `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` 정상 종료
* 전체 `re2dj_windows_vfs_runtime_probe`는 기존 GUI lifecycle 대기로 중단했으며,
  새 코드의 컴파일 및 분리된 오디오 종료 검증에는 영향을 주지 않았습니다.

### 남은 확인

사용자 환경에서 새 빌드로 JAM을 다시 재생하여 `*.audio.log`의 `track-reset=1`과
실제 청취 결과를 확인해야 합니다. 첫 payload가 원본 파일에서도 거의 무음인 점은
유지되므로, 재생 후 일정 시간 동안 이후 payload가 공급되는지도 함께 확인합니다.

## English

### Result

Although `jam.ezw` reads successfully through the CHD VFS, reusing the same SDL_mixer
track after `Stop` can leave output-stream data from the previous session. The backend now
creates a fresh `MIX_Track` for each explicit new Play on a stopped streaming voice and
reattaches the existing input `SDL_AudioStream` and cooked callback. Repeated Play while
already playing still preserves the queue and cursor.

### Changes

* `src/audio/sdl3_mixer_audio_backend.h/.cpp`
  * Added the `ResetTrack` helper.
  * Recreate the track for streaming playback after Stop.
  * Added the `track-reset` diagnostic marker.
* `src/tools/windows_vfs_runtime_probe/main.cpp`
  * Added a regression assertion for the Stop-then-reset marker.
* Added the design and work-order documents and updated the cumulative JAM analysis.

### Verification

* `cmd /c scripts\build_win32.bat` succeeded.
* `re2dj_ez2dj_keyboard_input_test` passed.
* `re2dj_ez2dancer_keyboard_input_test` passed.
* `re2dj_windows_product_loader_probe` passed.
* `re2dj_unit_tests` passed.
* `re2dj_windows_vfs_runtime_probe.exe --audio-exit-child` exited successfully.
* `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited successfully.
* The full `re2dj_windows_vfs_runtime_probe` was stopped because of its existing GUI
  lifecycle wait; this did not affect compilation or the separated audio-exit check.

### Remaining confirmation

The user should replay JAM with the new build and inspect `track-reset=1` in `*.audio.log`
alongside the audible result. Since the first payload is nearly silent in the original file
as well, the run should also confirm that later payload chunks are supplied after playback
continues.
