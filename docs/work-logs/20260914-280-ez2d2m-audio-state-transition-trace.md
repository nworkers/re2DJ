# 작업 로그: EZ2Dancer 오디오 상태 전환 진단

## 한국어

### 결과

`20260914-003504-572` 실행에서 JAM streaming buffer의 `Play` 직전 상태가
반복해서 `playing=0`이 되는 현상을 확인했습니다. 기존 trace에는 `Stop`과
`SetCurrentPosition` 호출이 없었으므로, 두 호출을 streaming buffer별 bounded trace로
추가했습니다.

### 변경 사항

- `Stop` 전후 guest/mixer 재생 상태, cursor, queue, HRESULT 기록
- `SetCurrentPosition`의 요청·이전·적용 위치와 전후 상태 기록
- buffer별 상태 전환 trace를 최대 64회로 제한
- Windows VFS runtime probe에 새 trace marker 회귀 검사 추가
- 설계·분석 문서 갱신

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- `re2dj_ez2dj_keyboard_input_test` 통과
- `re2dj_ez2dancer_keyboard_input_test` 통과
- `re2dj_windows_product_loader_probe` 통과
- `re2dj_unit_tests` 통과
- `re2dj_windows_vfs_runtime_probe.exe --audio-exit-child` 종료 검증 통과
- 전체 `re2dj_windows_vfs_runtime_probe`는 기존 GUI/audio lifecycle 대기에서 완료되지
  않아 중단했습니다. 새 trace 검사는 해당 probe의 DirectSound 구간에 포함되어 있으며,
  실제 `ez2d2m` 재실행으로 최종 확인해야 합니다.

### 다음 확인

사용자 실행 후 `.audio.log`에서 `directsound:stop`과
`directsound:set-position`을 찾습니다. `Stop` 직후 `track-playing-after=0`이면 원본의
명시적 정지 여부를 호출 순서로 확인하고, `SetCurrentPosition` 직후 queue가 다시
`360448`로 채워지면 위치 변경에 따른 의도된 streaming restart로 분류합니다.

## English

### Result

Run `20260914-003504-572` repeatedly shows `playing=0` immediately before JAM streaming
`Play`. Because the existing trace had no `Stop` or `SetCurrentPosition` records, bounded
transition tracing was added for streaming buffers.

### Changes

- Record guest/mixer playback state, cursor, queue, and HRESULT around `Stop`.
- Record requested, previous, and applied positions plus state around `SetCurrentPosition`.
- Limit state-transition records to 64 per buffer.
- Add regression checks for the new trace markers to the Windows VFS runtime probe.
- Update design and analysis documentation.

### Verification

- `cmd /c scripts\\build_win32.bat` succeeded.
- `re2dj_ez2dj_keyboard_input_test` passed.
- `re2dj_ez2dancer_keyboard_input_test` passed.
- `re2dj_windows_product_loader_probe` passed.
- `re2dj_unit_tests` passed.
- `re2dj_windows_vfs_runtime_probe.exe --audio-exit-child` exited successfully.
- The full `re2dj_windows_vfs_runtime_probe` remained in its existing GUI/audio lifecycle
  wait and was interrupted. The new trace checks are in the probe's DirectSound section;
  final confirmation requires another real `ez2d2m` run.

### Next observation

After the user captures a run, inspect `.audio.log` for `directsound:stop` and
`directsound:set-position`. A `track-playing-after=0` after `Stop` identifies an explicit
stop transition, while a queue returning to `360448` after `SetCurrentPosition` identifies
the intended position-change streaming restart.
