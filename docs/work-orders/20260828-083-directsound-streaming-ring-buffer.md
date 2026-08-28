# DirectSound streaming/ring-buffer 동기화 작업 지시

관련 설계: [DirectSound streaming/ring-buffer 동기화](../design/20260828-083-directsound-streaming-ring-buffer.md)

## 한국어

### 상태

**완료.** static/streaming voice를 분리하고 committed snapshot의 dirty circular 구간만 SDL stream에 전달했다. runtime probe, Windows x86 build, CTest와 0 dB 원본 실행에서 검증했다.

### 작업

1. SDL audio backend voice를 static `MIX_Audio`와 streaming `SDL_AudioStream` 경로로 분리한다.
2. streaming Play에서 현재 cursor부터 ring 한 바퀴를 큐잉하고 재생 중 Unlock region을 순서대로 추가한다.
3. streaming cursor, SetCurrentPosition, Stop/restart와 독립 duplicate voice 상태를 보존한다.
4. DirectSound facade trace에 lock offset, 두 region 길이, streaming queue 반영 결과를 bounded하게 추가한다.
5. unit/runtime probe를 확장하고 Windows x86 build와 CTest를 통과시킨다.
6. 실제 원본을 0 dB로 실행하여 전체 곡 진행과 음량을 확인하고 분석·아키텍처·TODO·구현 목록·작업 로그를 갱신한다.

### 완료 조건

- 정적 buffer는 기존 snapshot 재생 의미를 유지한다.
- 관찰된 streaming buffer의 Unlock PCM이 stale snapshot 반복 없이 SDL queue에 반영된다.
- wrap-around, cursor, Stop/restart와 duplicate 상태 테스트가 통과한다.
- 실제 실행에서 후속 청크의 peak/RMS와 시간 진행이 반복 관찰된다.
- 임시 기본 master gain 정책을 청취 결과에 따라 명시적으로 결정한다.

## English

### Status

**Complete.** Static and streaming voices are separated, and only dirty circular regions relative to the committed snapshot are forwarded to the SDL stream. Runtime probe, Windows x86 build, CTest, and a 0 dB original-executable run verify the result.

### Tasks

1. Split backend voices into static `MIX_Audio` and streaming `SDL_AudioStream` paths.
2. Queue one ring revolution from the current cursor at streaming Play and append successful Unlock regions in order while playing.
3. Preserve streaming cursor, SetCurrentPosition, Stop/restart, and independent duplicate voice state.
4. Add bounded lock offset, two-region length, and streaming queue-result fields to the DirectSound trace.
5. Extend unit/runtime probes and pass the Windows x86 build and CTest.
6. Run the original at 0 dB, verify complete-song progression and loudness, then update analysis, architecture, TODO, implemented items, and the work log.

### Completion criteria

Static buffers retain snapshot semantics; observed streaming Unlock PCM reaches the SDL queue without repeating the stale snapshot; wrap, cursor, restart, and duplicate-state tests pass; a real run repeatedly observes later-chunk levels and timeline progression; and the temporary default master-gain policy is explicitly resolved from listening evidence.
