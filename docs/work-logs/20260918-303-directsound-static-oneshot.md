# 작업 303 작업 로그 — 일회성 DirectSound 버퍼의 스트리밍 오분류 수정 / Task 303 work log — Fixing one-shot DirectSound buffers misclassified as streaming

설계: [20260918-303-directsound-static-oneshot.md](../design/20260918-303-directsound-static-oneshot.md)
작업 지시: [20260918-303-directsound-static-oneshot.md](../work-orders/20260918-303-directsound-static-oneshot.md)
배경: [구형 DirectSound secondary buffer 계약](../kb/legacy-directsound-buffer.md)

## 한국어

### 변경 전 수집

일곱 타깃을 `--audio-volume-trace`로 실행해 버퍼 생성·재생 플래그를 대조했다.

| 타깃 | 링 버퍼 (360,448, `looping=1`) | 효과음 | 변경 전 효과음 분류 |
| --- | --- | --- | --- |
| `ez2dj1st` | `0x140c6` | `0x140c2` | **스트리밍** |
| `ez2dj1stse` | `0x140c6` | `0x140e2` | **스트리밍** |
| `ez2dj2nd`, `ez2dj3rd`, `ez2dj4th`, `ez2dj5th` | `0x140c0` | `0x40e0` | 일반 |
| `ez2d2m` | `0x140c0` | `0x40c0` | 일반 |

설계의 가정(링은 모두 반복 재생, 효과음만 오분류)과 일치했다. 설계가 "영향 가능"으로 본 3rd는 영향이 없었고, 대신 1st Tracks가 같은 문제를 갖고 있었다. 설계 문서에 정정을 덧붙였다.

### 구현

* `re2dj::audio::IsStreamingBufferDescription` — 분류 규칙을 플랫폼 공용 함수로 옮겼다. 크기 360,448이면 링, 그 밖에는 `LOCHARDWARE`·`GETCURRENTPOSITION2`가 있으면서 `STATIC`이 없을 때만 링이다. DirectSound 파사드의 `is_streaming()`이 이 함수를 쓴다.
* `Sdl3MixerAudioBackend` — 스트리밍 음성을 반복 없이 재생하면 시작 위치부터 버퍼 끝까지만 공급(`stream_once`, `stream_once_remaining`)하고, `MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN`을 `!looping`으로 줘 트랙이 스스로 멈춘다. 반복 재생은 그대로다.
* 공용 함수 소스를 core와 injected runtime 양쪽 빌드에 추가했다. 처음에는 runtime 쪽이 빠져 `LNK2019`가 났다.

### 문서 확인 결과

Microsoft Learn 확인 결과 `DSBPLAY_LOOPING`이 없으면 버퍼가 끝에서 멈춘다는 점은 계약이다. 반면 `DSBCAPS_STATIC`은 사운드 카드 메모리 배치 요청일 뿐이고 "한 번 재생"을 뜻하지 않는다. 그래서 분류 규칙을 **관찰에 맞춘 휴리스틱**으로 KB와 코드 주석에 적었다. 반복 없는 스트리밍 재생이 멈추게 한 변경이 규칙 이탈에 대한 안전장치다.

### 검증

* Windows x86 Debug 빌드 성공.
* 단위 테스트: `checks: 1819, failures: 0`. 관찰된 플래그 조합마다 분류를 검사한다.
* `ctest`: 5/5 통과(원래 멈추는 `re2dj_windows_vfs_runtime_probe` 제외).
* `re2dj_audio_stream_progress_test`에 반복 없는 스트리밍 재생이 1초 안에 멈추는지 검사를 추가했다. **변이 확인:** 백엔드 수정을 되돌리면 `Non-looping streaming play never halted`로 실패(exit 4)하고, 복구하면 통과한다. 첫 변이 시도에서는 치환이 적용되지 않아 통과로 잘못 보였고, 스크립트로 치환을 단언한 뒤 다시 확인했다.
* 변경 후 수집: 1st·1st SE의 효과음이 모두 `is_streaming=0`, 링만 `is_streaming=1`. 2nd~5th·`ez2d2m`의 분류는 변경 전과 같고 링 재생은 모두 `looping=1`.
* 사용자 청취: 1st SE 효과음 반복이 사라졌고 다른 타깃 배경음에 이상이 없음을 확인.

### 남은 것

* `ez2dj6th` 자식 프로세스 오디오는 수집하지 않았다(비목표).

## English

### Before-change collection

Seven targets were run with `--audio-volume-trace`. Every ring was 360,448 bytes and played with `looping=1`: `0x140c6` in `ez2dj1st` and `ez2dj1stse`, `0x140c0` elsewhere. Effects were `0x140c2` in `ez2dj1st` and `0x140e2` in `ez2dj1stse`, both **classified streaming**, and `0x40e0` in 2nd through 5th and `0x40c0` in `ez2d2m`, both classified normal. This matched the design's assumption; 3rd, which the design thought might be affected, was not, while 1st Tracks had the same problem. A correction was appended to the design.

### Implementation

* `re2dj::audio::IsStreamingBufferDescription` moves the rule into a platform-neutral function: 360,448 bytes is a ring, and otherwise only `LOCHARDWARE` or `GETCURRENTPOSITION2` without `STATIC` is. The DirectSound facade's `is_streaming()` calls it.
* `Sdl3MixerAudioBackend` feeds a non-looping streaming play only from its start position to the end of the buffer (`stream_once`, `stream_once_remaining`) and sets `MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN` to `!looping`, so the track halts on its own; looping plays are unchanged.
* The shared source was added to both the core and the injected runtime; leaving the latter out first produced `LNK2019`.

### What the documentation says

Microsoft Learn makes stopping at the end without `DSBPLAY_LOOPING` a contract, but defines `DSBCAPS_STATIC` only as a request for on-board sound card memory, not one-shot playback. The rule is therefore recorded in the KB and the code comment as a **heuristic fitted to observation**, with the non-looping stop as the safeguard should it drift.

### Verification

* Windows x86 Debug build succeeded.
* Unit tests: `checks: 1819, failures: 0`, checking the classification of every observed flag combination.
* `ctest`: 5/5 passed, excluding the known hanging `re2dj_windows_vfs_runtime_probe`.
* `re2dj_audio_stream_progress_test` now checks that a non-looping streaming play halts within a second. **Mutation check:** reverting the backend fix fails with `Non-looping streaming play never halted` (exit 4), and restoring it passes. The first mutation attempt did not actually apply its substitution and looked like a pass; it was redone with a script asserting the substitution.
* After-change collection: every 1st and 1st SE effect is `is_streaming=0` with only rings streaming; 2nd through 5th and `ez2d2m` classify as before, with every ring play `looping=1`.
* User listening: the 1st SE effect repeat is gone, and other targets' background audio is unaffected.

### Remaining

* `ez2dj6th` child-process audio was not collected (a non-goal).
