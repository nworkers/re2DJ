# 작업 지시서: EZ2Dancer 오디오 상태 전환 진단

## 한국어

설계 문서 [20260914-280](../design/20260914-280-ez2d2m-audio-state-transition-trace.md)에
따라 JAM streaming buffer의 명시적 정지·위치 변경 호출을 관찰할 수 있도록 진단을
추가합니다.

### 작업

* DirectSound streaming facade의 `Stop` 전후 상태를 bounded trace로 기록합니다.
* `SetCurrentPosition`의 요청 위치와 전후 cursor·재생 상태·queue를 기록합니다.
* Windows VFS runtime probe에 두 trace marker의 회귀 검사를 추가합니다.
* 분석 문서와 작업 로그에 실제 실행 결과를 반영합니다.

### 검증

* `cmd /c scripts\build_win32.bat`
* `re2dj_windows_vfs_runtime_probe`
* DirectSound/audio 관련 CTest
* 사용자가 실행한 `ez2d2m`의 새 `.audio.log`에서 상태 전환 순서 확인

## English

Following [design 20260914-280](../design/20260914-280-ez2d2m-audio-state-transition-trace.md),
add bounded diagnostics for explicit stop and position changes on the JAM streaming
buffer.

### Tasks

* Record streaming DirectSound facade state before and after `Stop`.
* Record requested position and cursor, playback state, and queue before and after
  `SetCurrentPosition`.
* Add regression checks for both trace markers to the Windows VFS runtime probe.
* Update the analysis document and work log with the runtime result.

### Verification

* `cmd /c scripts\build_win32.bat`
* `re2dj_windows_vfs_runtime_probe`
* DirectSound/audio-related CTest tests
* Inspect the new `ez2d2m` `.audio.log` for transition ordering
