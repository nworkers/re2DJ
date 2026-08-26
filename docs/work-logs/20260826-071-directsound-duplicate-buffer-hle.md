# DirectSound duplicate buffer HLE 작업 로그

관련 설계: [DirectSound duplicate buffer HLE](../design/20260826-071-directsound-duplicate-buffer-hle.md)
관련 작업 지시: [DirectSound duplicate buffer HLE 작업 지시](../work-orders/20260826-071-directsound-duplicate-buffer-hle.md)

## 상태

**완료.** 공용 shared PCM과 독립 재생 상태, Windows facade와 SDL voice, probe와 단위 테스트를 구현했다.

## 구현

- `LegacyAudioBuffer` sample storage를 `shared_ptr<vector<byte>>`로 바꾸고 `Duplicate()`가 PCM은 공유하되 cursor, controls를 값으로 복제하며 Play/loop 상태는 정지 상태로 시작하게 했다.
- `DirectSoundFacade::DuplicateSoundBuffer`는 facade 소유 secondary buffer만 수용하고 primary/잘못된 입력을 거절한다. duplicate마다 새 COM refcount와 SDL mixer voice를 만들고 source/result, flags, byte count marker를 남긴다.
- runtime probe는 초기 volume 상속, duplicate volume 변경의 source 비영향, 독립 Play/Stop을 검증한다. 단위 테스트는 양방향 PCM write-through와 상태 독립성을 검증한다.

## 검증

- warnings-as-errors Windows x86 build와 CTest 2/2 성공
- warnings-as-errors Windows x64 build와 CTest 1/1 성공
- canonical trace `20260826-110206-895.jsonl`: secondary 126개, duplicate 70회, Play 84회, DrawPrimitive 37,937회, AV/OpenGL failure/SDL error 0회
- canonical trace `20260826-110358-397.jsonl`: secondary 126개, duplicate 47회, Play 60회, DrawPrimitive 36,111회, AV/OpenGL failure/SDL error 0회
- 두 실행 모두 새 안정 실패 경계 없이 메인 루프를 유지해 증거 확보 후 수동 종료했다. 원본 HDD는 읽기 전용으로 유지했다.
- 누적 문서 점검에서 `IMPLEMENTED.md`의 최신 두 작업이 작업 번호·근거 링크 없는 영문 항목으로만 보이고 `TODO.md`에 이미 완료한 원본 `.text` 안정화가 남은 문제를 확인했다. 작업 070·071과 현재 메인 루프 도달점을 한·영 최신 이정표로 명시하고 TODO를 사용자 검증과 VFS write/overlay 검증으로 정리했다.

---

# DirectSound Duplicate Buffer HLE Work Log

**Complete.** LegacyAudioBuffer now shares PCM storage while duplicated instances copy controls/cursor into independent playback state. Each Windows duplicate owns a separate COM lifetime and SDL mixer voice. Unit tests cover shared write-through and state isolation; the x86 probe covers inherited controls plus independent Play/Stop.

Warnings-as-errors x86 and x64 builds pass with CTest 2/2 and 1/1. Final traces 20260826-110206-895.jsonl and 20260826-110358-397.jsonl record 70/47 duplicates, 84/60 Play calls, and 37,937/36,111 DrawPrimitive calls with zero access violations, OpenGL failures, or SDL errors. Both remain in the main loop and were stopped manually after evidence collection; original HDD assets remained read-only.

A cumulative-document review found that IMPLEMENTED exposed the two latest tasks only as unlinked English bullets while TODO still listed the already-completed original `.text` stabilization. The indexes now identify Tasks 070 and 071 with evidence links, state the current main-loop milestone bilingually, and limit active TODO work to user-visible validation plus VFS write/overlay verification.
