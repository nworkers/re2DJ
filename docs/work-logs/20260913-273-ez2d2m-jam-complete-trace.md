# 작업 로그: complete trace 기반 EZ2Dancer JAM 음원 진단
# Work Log: EZ2Dancer JAM Audio Diagnosis from the Complete Trace

## 한국어

### 범위

이 작업은 사용자가 실제 JAM 플레이를 포함해 수집한
"20260913-232223-242.jsonl" 및 부속 진단 로그를 분석하는 문서 작업입니다. 소스 코드,
원본 바이너리, 원본 게임 자산은 수정하거나 저장소에 추가하지 않았습니다.

### 조사 결과

1. 실행 결과에 "graphics_draw_diagnostics":true가 포함되고 대형 ".ddraw.log"가
   생성되어 complete diagnostics 모드가 실제로 활성화되었습니다.
2. VFS는 "songs\\jam"으로 전환한 뒤 "jam.ezw"를
   "chd://ez2dancer/songs/jam/jam.ezw"에 성공적으로 매핑했습니다.
3. JAM 음원 handle의 초기 read 세 건은 각각 4,096, 356,352, 4,096바이트를 요청했고,
   모두 요청 바이트 수만큼 전달되었으며 오류 코드는 0입니다. 후속 22,528바이트 read도
   성공으로 기록됩니다.
4. DirectSound streaming buffer "1BD91C40"은 2채널/44.1 kHz/16-bit/360,448바이트로
   생성되었고 looping play와 streaming start까지 진행되었습니다. first-play와 이후
   streaming unlock의 peak/RMS가 모두 0이 아니므로 PCM 데이터는 무음이 아닙니다.
5. "356,352 + 4,096 = 360,448"이라는 크기 일치와 이벤트 순서는 해당 DirectSound
   buffer가 JAM 스트림일 가능성을 강하게 뒷받침합니다. 다만 현재 로그에는 VFS와
   DirectSound를 잇는 correlation ID가 없어 일대일 확정은 아닙니다.

### 결론

이번 실행에서는 JAM 음원이 파일시스템, CHD 매핑, EZW read에서 실패한 것이 아닙니다.
데이터는 오디오 HLE의 스트리밍 buffer까지 도달했습니다. 실제 청취가 계속 실패한다면
다음 조사 대상은 SDL3/backend device 출력, stream mixer/lifecycle, 또는 곡 전환·페이드
중 volume 상태입니다. 이번 작업에서는 코드 수정 없이 이 결론과 불확실성을
"docs/analysis/ez2d2m-chd-filesystem.md"에 누적했습니다.

### 검증

- "git diff --check": 문서 변경 후 수행
- 변경 범위: 분석 문서, 기존 작업 지시서 체크리스트, 작업 로그
- 코드 빌드: 코드 변경이 없어 수행하지 않음

## English

### Scope

This was a documentation-only analysis of the user-provided
"20260913-232223-242.jsonl" run and its companion diagnostic logs, including an actual JAM
play attempt. No source code, original executable, or original game asset was modified or
added to the repository.

### Findings

1. The launch result included "graphics_draw_diagnostics":true and produced a large
   ".ddraw.log", confirming that complete diagnostics were active.
2. The VFS changed the current directory to "songs\\jam" and successfully mapped "jam.ezw"
   to "chd://ez2dancer/songs/jam/jam.ezw".
3. The first three reads on the JAM audio handle requested 4,096, 356,352, and 4,096 bytes;
   each transferred the full requested amount with error code 0. Later 22,528-byte reads
   also succeeded.
4. DirectSound created streaming buffer "1BD91C40" as a 2-channel, 44.1 kHz, 16-bit,
   360,448-byte buffer and reached looping play and streaming start. Non-zero peak/RMS values
   in first-play and subsequent streaming-unlock measurements show that the PCM was not
   silent.
5. The size match "356,352 + 4,096 = 360,448" and event ordering strongly support that this
   DirectSound buffer is the JAM stream. The logs lack a shared VFS-to-DirectSound
   correlation ID, so this is not a one-to-one proof.

### Conclusion

This run does not show a JAM failure at the filesystem, CHD mapping, or EZW read boundary.
The data reached the audio HLE streaming buffer. If the track remains inaudible, the next
targets are SDL3/backend device output, stream mixer/lifecycle handling, or volume state
during song transitions and fades. No code was changed; the conclusion and uncertainty were
added to "docs/analysis/ez2d2m-chd-filesystem.md".

### Verification

- Ran "git diff --check" after the documentation changes.
- Changed only the analysis document, the existing work-order checklist, and this work log.
- No code build was run because the task made no code changes.
