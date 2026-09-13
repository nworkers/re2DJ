# 작업 지시서: EZ2Dancer 정지 streaming cursor 동기화

## 한국어

설계 문서 [20260914-281](../design/20260914-281-ez2d2m-stopped-stream-cursor.md)에
따라 정지된 SDL streaming track의 stale cursor가 DirectSound 위치 조회를 덮어쓰지
않도록 수정합니다.

### 작업

* 정지된 streaming voice의 `PositionBytes`가 `LegacyAudioBuffer` cursor를 반환하도록
  수정합니다.
* streaming probe에 `cursor-after` 위치 동기화 검사를 추가합니다.
* 분석 문서와 작업 로그를 갱신합니다.

### 검증

* `cmd /c scripts\build_win32.bat`
* DirectSound/audio 관련 CTest
* streaming probe의 `requested=12` 위치 동기화 확인
* 사용자 실행 JAM 로그 재확인

## English

Following [design 20260914-281](../design/20260914-281-ez2d2m-stopped-stream-cursor.md),
prevent a stopped SDL streaming track's stale cursor from overriding the DirectSound
position query.

### Tasks

* Return the `LegacyAudioBuffer` cursor from `PositionBytes` for stopped streaming voices.
* Add a `cursor-after` synchronization assertion to the streaming probe.
* Update the analysis document and work log.

### Verification

* `cmd /c scripts\\build_win32.bat`
* DirectSound/audio-related CTest tests
* Confirm `requested=12` position synchronization in the streaming probe
* Recheck a user-captured JAM run
