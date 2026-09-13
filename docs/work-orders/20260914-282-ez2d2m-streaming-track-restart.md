# 작업 지시서: EZ2Dancer 스트리밍 트랙 재시작 수명 수정

## 한국어

설계 문서 [20260914-282](../design/20260914-282-ez2d2m-streaming-track-restart.md)에
따라 `Stop` 이후 SDL_mixer track을 재사용하여 이전 output stream이 남을 수 있는
문제를 수정합니다.

### 작업 항목

* streaming voice의 명시적 재시작 경로에 track 재생성 helper를 추가합니다.
* 새 track에 cooked callback과 기존 input stream을 연결하고 controls를 다시 적용합니다.
* 이미 재생 중인 반복 `Play`의 continuation 의미와 비스트리밍 경로를 유지합니다.
* streaming probe에 Stop 후 새 track 재생성 회귀 검증을 추가합니다.
* 누적 JAM 분석 문서와 작업 로그를 갱신합니다.

### 검증

* `cmd /c scripts\build_win32.bat`
* DirectSound/audio 관련 CTest
* Windows x86 streaming runtime probe
* 새 track reset marker 및 JAM cooked PCM 로그 확인

## English

Following design [20260914-282](../design/20260914-282-ez2d2m-streaming-track-restart.md),
fix the reuse of an SDL_mixer track after `Stop`, which can leave converted output from
the previous stream session buffered.

### Tasks

* Add a track-recreation helper to the explicit streaming restart path.
* Reattach the cooked callback and existing input stream, then reapply controls.
* Preserve repeated-`Play` continuation semantics and the non-streaming path.
* Add a Stop-then-fresh-track regression check to the streaming probe.
* Update the cumulative JAM analysis and work log.

### Verification

* `cmd /c scripts\build_win32.bat`
* DirectSound/audio-related CTest tests
* Windows x86 streaming runtime probe
* Inspect the track-reset marker and JAM cooked-PCM trace
