# 작업 로그: EZ2Dancer post-mix 진단 샘플링 보완

## 한국어

### 새 로그 분석

사용자가 제공한 `20260913-235103-595` 실행을 확인한 결과 다음이 확인되었다.

- `sdl3:backend:ready=1:playback-device=1:postmix=1`이므로 SDL playback device가
  선택되었고 post-mix callback도 등록되었다.
- JAM 실행 시 `jam.ezw`가 `songs/jam` 경로에서 CHD로 열리고, 관련 읽기가 성공했다.
- streaming `sdl3:play`는 `configured=1`, `played=1`, `track-playing=1`이며 queue가
  360448에서 감소했다.
- 기존 post-mix 샘플 16회는 게임 오디오 재생 전에 모두 0 PCM으로 기록되어, JAM PCM의
  최종 출력 여부를 확인할 수 없었다.

### 변경 내용

post-mix 진단 횟수를 전체 callback 기준으로 제한하지 않고, 초기 무음 callback은 4회,
non-zero PCM callback은 16회까지 별도로 기록하도록 변경했다. 다음 실행에서는
`sdl3:postmix:audible=1`의 peak/RMS가 실제 mixer 출력 도달 여부를 보여준다.

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- 관련 CTest 4개 성공
- 실제 JAM 재생 검증은 개선된 빌드로 새 로그를 받아야 한다.

## English

### New-log analysis

The user-provided run `20260913-235103-595` confirms:

- `sdl3:backend:ready=1:playback-device=1:postmix=1`, so an SDL playback device was
  selected and the post-mix callback was registered.
- `jam.ezw` was opened from the `songs/jam` CHD path and its related reads succeeded.
- The streaming `sdl3:play` record reports `configured=1`, `played=1`, and
  `track-playing=1`; the queue later decreases from 360448 bytes.
- The previous 16 post-mix samples were all zero PCM before game audio playback, so they
  did not establish whether JAM PCM reached the final output.

### Change

Post-mix sampling is no longer limited by one global callback count. It now records up to
four initial silent callbacks and up to sixteen non-zero PCM callbacks independently. The
next run's `sdl3:postmix:audible=1` peak/RMS values will show whether PCM reached the mixer
output.

### Verification

- `cmd /c scripts\\build_win32.bat` succeeded.
- Four related CTest cases passed.
- Real JAM playback must be retested with the improved build.
