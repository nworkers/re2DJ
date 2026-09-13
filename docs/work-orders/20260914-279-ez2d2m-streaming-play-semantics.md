# 작업 지시서: EZ2Dancer streaming Play 의미 보정

## 한국어

설계 문서 [20260914-279](../design/20260914-279-ez2d2m-streaming-play-semantics.md)에
따라 DirectSound streaming buffer의 반복 `Play` 재시작을 제거합니다.

### 작업

- backend `Play`가 이미 재생 중인 streaming voice를 감지하도록 합니다.
- 재생 중인 streaming voice의 일반 `Play`는 control 갱신만 수행합니다.
- `SetCurrentPosition`은 재생 중이면 track을 정지한 뒤 강제 재시작합니다.
- continuation/restart 진단을 추가합니다.
- 문서와 작업 로그를 갱신합니다.

### 검증

- `cmd /c scripts\build_win32.bat`
- DirectSound/audio 관련 CTest 및 product probe
- 새 trace의 queue/cursor 상태 확인

## English

Implement the repeated-`Play` correction described in the linked design document.

### Tasks

- Detect an already-playing streaming voice in backend `Play`.
- Make ordinary `Play` update controls only for an already-playing streaming voice.
- Stop and force a restart from the new cursor when `SetCurrentPosition` changes it.
- Add continuation/restart diagnostics.
- Update the design and work-log documents.

### Verification

- Run `cmd /c scripts\build_win32.bat`.
- Run the DirectSound/audio CTest and product probes.
- Inspect queue and cursor state in the new trace.
