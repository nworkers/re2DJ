# DirectSound streaming/ring-buffer 동기화 작업 로그

관련 설계: [DirectSound streaming/ring-buffer 동기화](../design/20260828-083-directsound-streaming-ring-buffer.md)

관련 작업 지시: [DirectSound streaming/ring-buffer 동기화 작업 지시](../work-orders/20260828-083-directsound-streaming-ring-buffer.md)

## 한국어

### 구현

SDL audio voice를 정적 `MIX_Audio`와 streaming `SDL_AudioStream` 경로로 분리했다. 관찰된 `DSBCAPS_LOCHARDWARE` secondary buffer의 Play는 current cursor부터 ring 한 바퀴를 복사해 queue와 committed snapshot을 만든다. 재생 중 Unlock은 현재 PCM과 snapshot을 frame 단위로 비교하고, 변경 frame 사이에서 가장 큰 unchanged circular gap을 제외한 최소 구간만 queue에 추가한다. Stop은 backend cursor를 보존하고 restart와 SetCurrentPosition은 해당 cursor에서 stream을 다시 준비한다.

DirectSound trace에는 lock offset, first/second 크기, dirty offset/크기, queue 크기와 refresh 결과를 최대 16회씩 기록한다. PCM 원문은 기록하지 않는다. runtime probe는 32바이트 ring의 offset 24, 16바이트 wrap write가 dirty offset 24, 길이 16으로 반영되는지와 Play/Stop/restart, cursor 범위, 기존 static/duplicate 계약을 검증한다.

### 구현 중 정정

첫 실제 실행 `20260828-151051-585.audio.log`에서 Unlock 인자 전체를 append한 초기 구현은 queue를 360,448바이트에서 5,448,476바이트까지 증가시켰다. 원본은 play cursor가 약 44–46KB씩 전진하는데도 매번 `DSBLOCK_ENTIREBUFFER`로 전체 ring을 Lock/Unlock했다. 따라서 Unlock 범위와 실제 변경 범위가 같다는 추정을 폐기하고 committed-snapshot dirty 비교로 교체했다.

### 최종 실행 증거

0 dB 최종 실행 `20260828-151817-074.audio.log`는 streaming Unlock 64회를 기록했다. 그중 초기 no-change 7회를 제외한 57회는 모두 정확히 45,056바이트 dirty 청크였다. offset은 `0, 45056, ... 315392, 0`으로 순환했고 queue는 251,160–358,684바이트에 머물렀다. 후속 PCM은 peak 약 0.98과 변화하는 RMS를 보였고 stale 첫 snapshot 반복과 무제한 queue 증가는 사라졌다. 대응 JSONL에는 access violation, controlled exit와 OpenGL failure가 없었다. 증거 수집을 위해 시작한 원본과 loader PID만 실행별로 종료했으며 HDD는 변경하지 않았다.

`-3333` 등 원본 DirectSound volume은 1/100 dB 계약대로 유지했다. 임시 제품 기본 master gain `+6 dB`도 유지하지만 더 올리지 않는다. 최종 체감 음량과 clipping은 사용자가 전체 곡과 효과음을 청취하는 작업 072 재검증에 남긴다.

### 검증

- Windows x86 warnings-as-errors 전체 build 통과
- CTest 3/3 통과
- dummy runtime probe의 static, streaming wrap, cursor, stop/restart와 duplicate 회귀 통과
- 실제 0 dB 실행의 64회 streaming Unlock, bounded queue와 오류 0건 확인

## English

### Implementation and correction

Split SDL voices into static `MIX_Audio` and streaming `SDL_AudioStream` paths. Play on the observed `DSBCAPS_LOCHARDWARE` buffer queues one ring revolution from the current cursor and stores a committed snapshot. Each Unlock compares PCM frames, excludes the largest unchanged circular gap, and queues only the minimal dirty interval. Stop preserves the backend cursor; restart and SetCurrentPosition rebuild the stream from that cursor.

The first real run, `20260828-151051-585.audio.log`, disproved the assumption that the Unlock range equals new PCM: appending each whole-buffer Unlock grew the queue from 360,448 to 5,448,476 bytes while the play cursor advanced only about 44–46 KB per update. The committed-snapshot design replaced that implementation.

### Final evidence

Final 0 dB run `20260828-151817-074.audio.log` records 64 streaming Unlocks. Excluding seven initial no-change updates, all 57 dirty updates are exactly 45,056 bytes, cycling through offsets `0, 45056, ... 315392, 0`. The queue remains bounded from 251,160 through 358,684 bytes, later PCM reaches roughly 0.98 peak with changing RMS, and stale first-snapshot repetition and unbounded latency are removed. The matching JSONL has no access violation, controlled exit, or OpenGL failure. Only the original and loader PIDs started for evidence collection were stopped; the HDD remained unchanged.

Original DirectSound values such as `-3333` retain their hundredths-of-a-decibel meaning. The temporary `+6 dB` product master default is retained without further increase. Final perceived loudness and clipping across the complete song and effects remain a Task 072 user-listening check.

Windows x86 warnings-as-errors build and all three CTest cases pass. The dummy runtime probe covers static playback, streaming wrap, cursor bounds, Stop/restart, and duplicate regressions.
