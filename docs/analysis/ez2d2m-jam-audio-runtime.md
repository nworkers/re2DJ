# EZ2Dancer JAM 오디오 런타임 분석

## 최신 결론: 20260914-285 / Latest conclusion

### 한국어

- **확인됨:** 실행 `20260914-013700-710`에서 `jam.ezw`의 CHD VFS open과 초기
  `4096 + 356352 + 4096`바이트 read, 후속 `22528`바이트 read가 모두 성공했습니다.
- **확인됨:** JAM에 해당하는 마지막 360448바이트 streaming Play는 gain 1.0 및 재생 상태를
  유지했지만 cooked PCM peak가 약 `0.000061`로 고정되어 사실상 무음이었습니다.
- **확인됨:** 같은 구간에 전체 ring Unlock이 계속 들어왔지만 작업 284의 committed snapshot
  비교는 동일한 바이트를 새 시간 구간으로 처리하지 못했습니다.
- **확인됨:** 소비량 기반 보충은 오래된 ring을 미리 FIFO에 넣어 반복을 만들고, snapshot
  기반 보충은 동일 PCM 진행을 누락합니다. append-only FIFO는 제자리 갱신 DirectSound
  ring의 시간 의미를 보존하지 못합니다.
- **확인됨:** 작업 285는 SDL get callback이 synchronized shadow ring을 현재 cursor에서
  on-demand로 읽도록 변경했으며, 합성 테스트에서 한 ring 이상 진행, 동일 PCM Unlock,
  반대 극성 PCM 갱신, Stop/restart가 통과했습니다.
- **미확정:** 실제 JAM의 연속 청취 결과는 수정 빌드로 다시 확인해야 합니다.

### English

- **Confirmed:** in run `20260914-013700-710`, CHD VFS open for `jam.ezw`, the initial
  `4096 + 356352 + 4096`-byte reads, and later `22528`-byte reads all succeeded.
- **Confirmed:** the final 360448-byte streaming Play associated with JAM remained playing
  at gain 1.0, but its cooked PCM peak stayed near `0.000061`, effectively silent.
- **Confirmed:** complete-ring Unlock calls continued in the same interval, but task 284's
  committed snapshot could not represent identical bytes as a new timeline interval.
- **Confirmed:** consumption-based refill prequeued stale ring data and caused repetition,
  while snapshot refill omitted identical-PCM progress. An append-only FIFO does not preserve
  the timing semantics of an in-place DirectSound ring.
- **Confirmed:** task 285 changes the SDL get callback to pull on demand from a synchronized
  shadow ring at the current cursor. Its synthetic test passes more than one ring of progress,
  identical-PCM Unlock, opposite-polarity PCM update, and Stop/restart.
- **Unresolved:** continuous audible JAM playback still requires confirmation with this build.

이하 내용은 이전 조사 결과이며 최신 결론과 구분합니다.
The remaining sections are earlier investigation history and are separate from the latest conclusion.

## 최신 결론: 20260914-284 / Latest conclusion

- **확인됨:** `011631-574`는 `track-reset=1` 적용 후 약 1~2초마다 초기 구간을
  반복합니다. `track-reset` 자체보다 그 뒤에 적용된 소비량 기반 보충이 원인이었습니다.
- **확인됨:** 소비량 기반 코드는 guest ring이 아직 갱신되지 않아도 ring 시작점부터
  같은 데이터를 다시 queue에 넣어 초기 구간을 반복시켰습니다.
- **확인됨:** committed snapshot 비교를 복원한 수정 빌드의 합성 테스트에서 변경 없는
  Unlock은 queue를 ring 크기 이상으로 늘리지 않고, 이후 변경된 tone은 cooked output에
  도달했습니다.
- **추정:** JAM의 후속 file chunk가 기록된 뒤 변경 구간만 queue에 추가하는 방식이 현재
  원본 ring 사용 패턴과 맞습니다.
- **미확정:** 실제 JAM 곡 전체에서 반복이 사라졌는지는 수정 빌드로 확인해야 합니다.

- **Confirmed:** `011631-574` repeats its initial segment about every 1–2 seconds with
  `track-reset=1`. The consumption-based refill added afterward caused this behavior.
- **Confirmed:** that code requeued the ring beginning even when the guest had not refreshed
  it, replaying the initial segment.
- **Confirmed:** the corrected build restores committed-snapshot comparison. Its synthetic
  test keeps unchanged Unlock calls within one ring and delivers changed tone data to cooked
  output.
- **Inferred:** queuing only changed ranges matches the observed guest ring pattern, in which
  later file chunks are written into the ring before they are supplied.
- **Unresolved:** whether the real JAM track is fully continuous requires a fresh run.

이하 내용은 이전 조사 경과이며 최신 결론과 구분합니다.
The remaining sections record earlier investigation history, not the latest conclusion.

## 한국어

### 2026-09-14 실행 결과

대상 로그는 `logs/windows_x86_launcher_probe/ez2d2m/20260914-001658-331`입니다.

#### 확인됨

- VFS에서 `jam.ezw`가 `chd://ez2dancer/songs/jam/jam.ezw`로 매핑되고 open/read가
  `success=1:error=0`으로 완료되었습니다.
- 초기 오디오 read `4096`, `356352`, `4096` bytes가 모두 요청량만큼 전송되었습니다.
- SDL backend가 `playback-device=1`, `postmix=1`로 초기화되었습니다.
- JAM으로 추정되는 DirectSound streaming buffer `1BD91D70`에 대해 6회의 Play와
  streaming-start가 기록되었습니다.
- 같은 track의 SDL_mixer cooked PCM이 peak 약 `0.74`, RMS 약 `0.52`까지 기록되어
  PCM이 stream input과 track 변환 경계를 통과했습니다.

#### 확인된 HLE 불일치

현재 `Sdl3MixerAudioBackend::Play()`는 streaming buffer가 이미 재생 중인지 확인하지
않고 매번 SDL stream을 비운 후 `pos=0`부터 ring 전체를 다시 넣고 mixer track을
재시작합니다. 해당 실행에서 매번 `pos=0`, `queued=360448`이 기록된 것이 이를
뒷받침합니다.

Microsoft DirectSound 계약에 따르면 이미 재생 중인 buffer에 대한 `Play`는 성공하고
현재 위치에서 계속 재생합니다. 따라서 반복 `Play`에 의한 되감기는 HLE 동작이며,
파일시스템 오류나 JAM PCM 부재가 아닙니다.

#### 추정 및 미확정

- **추정:** 반복 `Play`가 buffer 시작의 거의 무음인 PCM을 계속 재공급하여 사용자가
  JAM을 무음 또는 간헐적인 소리로 인식했을 가능성이 큽니다.
- **미확정:** 수정 빌드에서 실제 게임이 충분한 시간 동안 정상적인 곡 진행을 하는지는
  사용자의 청취 검증이 필요합니다.
- **미확정:** 동일한 반복 `Play` 패턴이 모든 EZ2Dancer 곡과 EZ2DJ 버전에도 적용되는지는
  각 버전의 실행 로그로 확인해야 합니다.

### 2026-09-14 수정 범위

재생 중 streaming voice의 일반 `Play`는 control만 갱신하고 queue/cursor를 보존하도록
수정했습니다. `SetCurrentPosition`이 재생 중일 때는 먼저 track을 정지한 뒤 새 위치에서
ring을 공급하도록 하여 명시적인 cursor 이동은 유지했습니다. Windows x86 runtime probe는
반복 streaming `Play`가 continuation trace를 남기는지 검사합니다.

## English

### 2026-09-14 runtime result

The target logs are under
`logs/windows_x86_launcher_probe/ez2d2m/20260914-001658-331`.

#### Confirmed

- VFS maps `jam.ezw` to `chd://ez2dancer/songs/jam/jam.ezw`, and open/read completes
  with `success=1:error=0`.
- Initial audio reads of `4096`, `356352`, and `4096` bytes all transfer their requested
  sizes.
- The SDL backend initializes with `playback-device=1` and `postmix=1`.
- The DirectSound streaming buffer inferred to be JAM, `1BD91D70`, records six Play and
  streaming-start events.
- Cooked PCM for the same track reaches approximately `0.74` peak and `0.52` RMS, proving
  that PCM crosses the stream-input and track-conversion boundaries.

#### Confirmed HLE divergence

The current `Sdl3MixerAudioBackend::Play()` does not check whether a streaming buffer is
already playing. It clears the SDL stream, refills the entire ring from `pos=0`, and restarts
the mixer track on every call. The run records `pos=0` and `queued=360448` on each call,
supporting this conclusion.

Microsoft's DirectSound contract says that `Play` on an already-playing buffer succeeds and
continues from its current position. The repeated rewind is therefore HLE behavior, not a
filesystem failure or missing JAM PCM.

#### Inferred and unresolved

- **Inferred:** repeated `Play` likely keeps supplying the nearly silent PCM at the start of
  the buffer, making JAM sound silent or intermittent to the user.
- **Unresolved:** the user must listen to a sufficiently long run with the fix to confirm
  continuous song progression.
- **Unresolved:** whether every EZ2Dancer song and EZ2DJ version uses the same repeated-Play
  pattern requires per-version runtime traces.

### 2026-09-14 fix scope

Ordinary `Play` on an already-playing streaming voice now updates controls while preserving
the queue and cursor. When `SetCurrentPosition` is called during playback, the track is
stopped first and refilled from the new position, preserving explicit cursor changes. The
Windows x86 runtime probe checks for a continuation trace after repeated streaming `Play`.

Reference: [IDirectSoundBuffer::Play — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708933%28v%3Dvs.85%29)

### 2026-09-14 상태 전환 진단 추가

새 실행 로그 `20260914-003504-572`에서 JAM으로 추정되는 streaming buffer는
`streaming-start` 5회와 `streaming-continue` 0회를 기록했습니다. 각 `Play` 전에
`playing=0`이 관찰되었으므로, 이전 반복 `Play` 보정 경로가 실제 실행에서 사용된
증거는 없습니다. 이 로그만으로는 원본의 `Stop` 또는 `SetCurrentPosition` 호출인지,
다른 HLE 상태 전환인지 확정할 수 없습니다.

같은 실행에서 다음 경계는 계속 통과했습니다.

- **확인됨:** `jam.ezw`가 CHD VFS에서 성공적으로 열리고 읽혔습니다.
- **확인됨:** streaming Unlock의 backend refresh가 성공했고 queue가 유한한 범위에서
  유지되었습니다.
- **확인됨:** JAM streaming track의 cooked PCM에 비무음 peak/RMS가 기록되었습니다.
- **확인됨:** SDL playback device와 post-mix callback이 활성화되었습니다.
- **미확정:** 재생을 `playing=0`으로 만든 호출과 그 호출 직후 queue/cursor 변화.

이를 구분하기 위해 DirectSound streaming facade의 `Stop`과 `SetCurrentPosition`에
buffer별 최대 64회 bounded 상태 전환 trace를 추가했습니다. trace는 PCM 원문을
기록하지 않고 guest playing 상태, mixer track 상태, cursor, queue, HRESULT만 남깁니다.
재생 의미나 cursor 동작은 변경하지 않았습니다.

사용자 재현 후 `*.audio.log`에서 다음 marker를 확인합니다.

- `directsound:stop`
- `directsound:set-position`
- 각 marker의 `track-playing-before/after`, `cursor-before/after`, `queued-before/after`

### 2026-09-14 상태 전환 로그 결과와 cursor 수정

실행 로그 `20260914-004813-801`에서 다음을 확인했습니다.

- **확인됨:** JAM streaming buffer에 `Stop` 18회, `SetCurrentPosition` 24회,
  `streaming-start` 5회가 기록되었습니다.
- **확인됨:** 재생 중인 구간마다 `Stop` 전 상태가
  `guest-playing-before=1:track-playing-before=1`이고, 호출 후 두 상태가 모두 0이
  되었습니다.
- **확인됨:** `Stop` 뒤 `SetCurrentPosition(0)`은 `applied=0`을 기록했지만,
  `cursor-after`는 `331632`, `69376`, `158168`, `267536` 등 이전 mixer frame을
  계속 보고했습니다.
- **확인됨:** `jam.ezw` VFS open/read, streaming refresh, cooked PCM, SDL playback
  device/post-mix 경계는 이 실행에서도 성공했습니다.

원인은 정지된 SDL streaming track의 마지막 mixer playback frame을
`PositionBytes`가 계속 사용한 HLE 상태 불일치로 판단됩니다. 정지된 stream voice는
이제 SDL mixer frame을 읽지 않고 `LegacyAudioBuffer::current_position()`을 반환합니다.
재생 중인 cursor 계산과 다음 `Play`에서 ring을 다시 공급하는 동작은 유지됩니다.

probe에서 `requested=12:applied=12:cursor-after=12`를 회귀 검사하도록 추가했습니다.
이 수정이 JAM의 무음 자체를 해결했는지는 새 제품 실행에서 청취와 streaming trace를
다시 대조해야 합니다.

## 2026-09-14 State-transition log result and cursor fix

Run `20260914-004813-801` confirms:

- **Confirmed:** the JAM streaming buffer records 18 `Stop` calls, 24
  `SetCurrentPosition` calls, and 5 `streaming-start` events.
- **Confirmed:** during active playback, `Stop` begins with
  `guest-playing-before=1:track-playing-before=1` and leaves both states at zero.
- **Confirmed:** after `Stop`, `SetCurrentPosition(0)` reports `applied=0`, but
  `cursor-after` continues to report stale mixer frames such as `331632`, `69376`,
  `158168`, and `267536`.
- **Confirmed:** `jam.ezw` VFS open/read, streaming refresh, cooked PCM, and the SDL
  playback-device/post-mix boundaries still succeed in this run.

The cause is an HLE state mismatch: `PositionBytes` kept using the last mixer playback
frame for a stopped SDL streaming track. A stopped stream voice now returns
`LegacyAudioBuffer::current_position()` instead of querying the SDL mixer frame. Cursor
calculation while playing and ring refill on the next `Play` remain unchanged.

The probe now checks `requested=12:applied=12:cursor-after=12`. Whether this fixes the
audible JAM silence still requires a fresh product run and a listening/trace comparison.

## 2026-09-14 State-transition diagnostic addition

Run `20260914-003504-572` records five `streaming-start` events and zero
`streaming-continue` events for the streaming buffer inferred to be JAM. `playing=0`
appears before every `Play`, so this run does not show the repeated-`Play` continuation
path exercising. The log cannot yet determine whether the state was changed by the
original game's `Stop` or `SetCurrentPosition`, or by another HLE transition.

The following boundaries still pass in the same run:

- **Confirmed:** `jam.ezw` opens and reads successfully through the CHD VFS.
- **Confirmed:** streaming Unlock backend refresh succeeds and the queue remains bounded.
- **Confirmed:** the JAM streaming track produces non-silent cooked PCM peak/RMS samples.
- **Confirmed:** the SDL playback device and post-mix callback are active.
- **Unresolved:** which call caused `playing=0` and how cursor/queue changed immediately after it.

To distinguish these cases, the DirectSound streaming facade now emits at most 64 bounded
state-transition records per buffer for `Stop` and `SetCurrentPosition`. The records contain
no original PCM bytes and include guest playing state, mixer-track state, cursor, queue, and
HRESULT. Playback semantics and cursor behavior are unchanged.

After the user captures another run, inspect `*.audio.log` for:

- `directsound:stop`
- `directsound:set-position`
- `track-playing-before/after`, `cursor-before/after`, and `queued-before/after` on each marker

## 2026-09-14 JAM silence follow-up

### 한국어

실행 로그 `20260914-005510-851`을 추가로 확인한 결과, 다음 사실을 확인했습니다.

- **확인됨:** `jam.ezw`는 `chd://ez2dancer/songs/jam/jam.ezw`로 매핑되고 VFS open/read가
  성공했습니다. 원본 파일을 저장소나 overlay에 복사하지 않았습니다.
- **확인됨:** JAM으로 추정되는 `360448` byte streaming buffer에 대해 첫 `track-cooked`
  callback 이후 거의 무음에 가까운 callback이 이어졌습니다.
- **확인됨:** CHD에서 임시 경로로 덤프한 `jam.ezw`는 4096 byte header 뒤에 payload를
  가지며, payload의 첫 360448 bytes 자체도 거의 무음입니다. 따라서 이 현상만으로
  VFS read 실패를 의미하지는 않습니다.
- **확인됨:** 로컬 SDL_mixer 구현의 `MIX_StopTrack`은 `internal_stream`만 clear하고
  `MIX_Track`의 `output_stream`은 clear하지 않습니다. 현재 HLE는 Stop 후 동일 track을
  새 Play에 재사용하고 있었습니다.

### 판단 및 수정

위 사실을 종합하면 파일시스템 오류보다는 명시적 Stop 이후 SDL_mixer track output
수명이 다음 재생 세션과 섞일 수 있는 HLE 문제가 재현 조건에 포함되어 있었습니다.
streaming voice가 이미 input stream을 가진 상태에서 stopped track으로 새 Play를
받으면 기존 track을 폐기하고 새 track을 생성하도록 수정했습니다. caller-owned
`SDL_AudioStream`은 유지하고 새 track에 cooked callback과 input stream을 다시 연결하므로
ring buffer와 DirectSound cursor 정책은 바뀌지 않습니다. 이미 재생 중인 반복 Play는
기존 continuation 경로를 유지합니다.

수정 후 로그에는 `track-reset=1` marker가 남으며, Windows x86 runtime probe가 Stop 후
재생성 경로를 검사합니다. 사용자의 실제 JAM 청취 결과는 새 빌드로 재실행하여 최종
확인해야 합니다.

### English

Additional inspection of run `20260914-005510-851` confirms:

- **Confirmed:** `jam.ezw` maps to `chd://ez2dancer/songs/jam/jam.ezw`, and VFS open/read
  succeeds. The original file was not copied into the repository or overlay.
- **Confirmed:** the 360448-byte streaming buffer inferred to be JAM produces one audible
  first `track-cooked` callback followed by callbacks that are nearly silent.
- **Confirmed:** a temporary dump of `jam.ezw` shows a 4096-byte header followed by payload;
  the first 360448 payload bytes are themselves nearly silent. This alone does not indicate
  a VFS read failure.
- **Confirmed:** the local SDL_mixer implementation of `MIX_StopTrack` clears only
  `internal_stream`, not a track's `output_stream`. The HLE reused the same `MIX_Track` for
  a new Play after Stop.

### Assessment and fix

These facts identify a possible HLE lifetime mismatch on explicit Stop/restart, rather than
an asset path failure. When a streaming voice already has an input stream and a new Play
arrives for a stopped track, the backend now destroys the old track and creates a fresh one.
The caller-owned `SDL_AudioStream` is preserved and reattached with the cooked callback, so
ring-buffer input and DirectSound cursor policy remain unchanged. Repeated Play while the
voice is already playing keeps the existing continuation path.

The fix emits a `track-reset=1` marker, and the Windows x86 runtime probe checks the
Stop-then-recreate path. Final audible confirmation for JAM still requires a fresh run with
the new build.
