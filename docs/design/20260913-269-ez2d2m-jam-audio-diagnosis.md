# EZ2Dancer JAM 음원 파일시스템 진단 설계
# Design: EZ2Dancer JAM Audio Filesystem Diagnosis

## 한국어

### 목적

`ez2d2m`에서 `JAM` 곡의 소리가 나오지 않는 현상을 다음 세 경계로 나누어
확인합니다.

```mermaid
flowchart LR
    A[CHD/FAT32 파일 존재] --> B[Win32 VFS open/read]
    B --> C[원본 EZW 해석 및 DirectSound buffer]
    C --> D[SDL3 오디오 출력]
```

이번 진단은 원본 게임 코드와 CHD를 수정하지 않습니다. 먼저 음원 파일이 이미지에서
정상적으로 제공되는지 확인하고, 실제 `JAM` 플레이 로그가 확보된 뒤에야 VFS 문제와
오디오 HLE 문제를 구분합니다.

### 현재 확인된 사실

- CHD 내부 경로 `ez2dancer/Songs/Jam/jam.ezw`가 존재하고 크기는 `17,753,894`바이트입니다.
- 파일의 18바이트 헤더는 2채널, 44,100 Hz, 16-bit, block align 4, data length
  `17,753,876`으로 파일 크기와 정확히 맞습니다.
- `re2dj_chd_probe --dump`로 CHD에서 다시 꺼낸 `jam.ezw`와 기존 추출본은 크기와
  SHA-256이 일치합니다.
- 추출 트리의 `.ezw` 252개 모두 헤더 data length와 파일 크기가 일치합니다. 따라서
  `jam.ezw`만 FAT32 cluster chain에서 잘렸다는 증거는 없습니다.
- 기존 VFS 로그에는 `Songs\\DEMO1\\jam.abm`만 있고, 실제 곡 경로인
  `Songs\\Jam\\jam.ezw`의 open/read 기록은 없습니다. `jam.abm`은 데모 애셋이므로
  이 로그만으로 JAM 곡 재생을 확인할 수 없습니다.

### 판단 규칙

실제 곡을 시작한 로그에서 다음을 확인합니다.

1. `create-file` 요청이 `Songs\\Jam\\jam.ezw` 또는 bare `jam.ezw`로 발생하는지 확인합니다.
2. `stage=chd`, `success=1`과 `read-file-result`의 성공·전송 바이트를 확인합니다.
3. 같은 실행의 `.audio.log`에서 DirectSound buffer 생성, `first-play`, PCM peak/RMS를
   확인합니다.

open/read가 성공하면 파일시스템 경계는 원인에서 제외합니다. open 실패 또는 다른
경로 요청이면 프로파일의 current directory/path mapping을 조사합니다. read가 성공한
뒤에도 DirectSound 재생이나 유효한 PCM snapshot이 없으면 원본 EZW 해석 또는 오디오
HLE 경계를 조사합니다.

### 사용자 재현 절차

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe ez2d2m --audio-volume-trace --io-config .\config\ez2dancer-io.example.ini
```

게임에서 `JAM`을 선택하고 곡이 시작된 뒤 최소 10초 이상 진행한 다음 게임을 닫습니다.
그 실행의 `logs\\windows_x86_launcher_probe\\ez2d2m\\*.vfs.log`와 같은 이름의
`*.audio.log`를 대조합니다. 원본 음원 샘플은 로그에 기록하지 않습니다.

## English

### Purpose

Separate the missing `JAM` audio in `ez2d2m` into three boundaries: the asset's presence
inside CHD/FAT32, Win32 VFS open/read, and original EZW decoding followed by DirectSound
and SDL3 output.

This diagnosis does not modify the original game code or CHD. A VFS defect and an audio
HLE defect are only distinguished after a log covers the actual `JAM` play interval.

### Facts confirmed so far

- `ez2dancer/Songs/Jam/jam.ezw` exists in the CHD and is `17,753,894` bytes.
- Its 18-byte header describes 2 channels, 44,100 Hz, 16-bit PCM, block align 4, and
  `17,753,876` data bytes, exactly matching the file size.
- A file dumped again from the CHD with `re2dj_chd_probe --dump` has the same size and
  SHA-256 as the existing extracted copy.
- All 252 extracted `.ezw` files have a data length matching their file size, so there is
  no evidence that `jam.ezw` alone has a broken FAT32 cluster chain.
- Existing VFS logs contain `Songs\\DEMO1\\jam.abm`, but no open/read record for the actual
  `Songs\\Jam\\jam.ezw` path. The former is a demo asset and does not prove that the JAM
  track was started.

### Decision rules

For a log captured while the track is actually starting:

1. Find the `create-file` request for `Songs\\Jam\\jam.ezw` or bare `jam.ezw`.
2. Check for `stage=chd`, `success=1`, and successful `read-file-result` byte counts.
3. Check the matching `.audio.log` for DirectSound buffer creation, `first-play`, and
   non-zero PCM peak/RMS.

Successful open/read excludes the filesystem boundary. An open failure or unexpected path
keeps profile/current-directory mapping under investigation. Successful reads without a
DirectSound play or valid PCM snapshot point to original EZW interpretation or the audio
HLE boundary.

## 2026-09-13 SDL 출력 경계 구현

스트리밍 `MIX_PlayTrack` 옵션에 `MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false`를
적용했다. 입력 SDL_AudioStream이 일시적으로 비어도 track을 stopped 상태로 확정하지 않고,
다음 `Unlock`에서 공급되는 데이터를 계속 사용할 수 있게 하는 변경이다.

또한 DirectSound 생성 시 SDL_mixer backend의 상태를 bounded audio trace로 기록한다.
기록 항목은 mixer 준비 여부, 실제 playback device 사용 여부, post-mix callback 등록 여부,
mixer 출력 포맷과 sample rate/channel이다. post-mix callback에서는 처음 16회만 PCM peak/RMS를
기록하여 최종 mixer 출력까지 데이터가 도달했는지를 확인할 수 있다.

현재 memory-only mixer fallback 자체는 유지한다. 이 경로는 기존 headless probe와의
호환성을 보존하면서 `playback-device=0`으로 명시적으로 드러난다. 실제 JAM 실행에서
해당 값과 post-mix 기록을 확인한 뒤, 제품 실행 시 fallback을 오류로 처리할지 결정한다.

## 2026-09-13 SDL output-boundary implementation

Streaming `MIX_PlayTrack` now sets
`MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false`. A temporarily empty
`SDL_AudioStream` therefore does not permanently transition the track to stopped, and
data supplied by a later `Unlock` can continue playback.

DirectSound creation now connects bounded audio diagnostics to the SDL_mixer backend.
The trace records mixer readiness, physical playback-device availability, post-mix callback
registration, mixer output format, and sample rate/channel. The post-mix callback records
PCM peak/RMS for only the first 16 callbacks, which distinguishes data reaching the final
mixer output from data that only reached the stream queue.

The memory-only mixer fallback remains available for compatibility with headless probes, but
it is now visible as `playback-device=0`. A new real JAM run must determine whether the
product should reject that fallback instead of allowing a silent successful initialization.

## 2026-09-13 진단 로그 재검토

새 JAM 실행 로그에서는 `sdl3:backend:ready=1:playback-device=1:postmix=1`이 확인되었다.
따라서 해당 실행은 memory-only mixer fallback이 아니라 SDL playback device 경로를
사용했다. `sdl3:play`도 streaming track에 대해 `played=1`, `track-playing=1`을 기록했고,
이후 stream queue가 감소하여 mixer가 입력을 소비하고 있음도 확인된다.

그러나 post-mix trace의 제한 횟수 16회가 DirectSound streaming 재생 전에 모두 0 PCM으로
소진되었다. 따라서 현재 로그만으로 JAM PCM이 최종 mixer 출력에 도달했는지는 판단할 수
없다. post-mix 계측은 초기 무음 callback 일부와 첫 non-zero PCM callback들을 별도로
수집하도록 보완해야 한다.

## 2026-09-13 Diagnostic-log review

The new JAM run reports `sdl3:backend:ready=1:playback-device=1:postmix=1`, so this
run used the SDL playback-device path rather than the memory-only fallback. `sdl3:play`
also reports `played=1` and `track-playing=1` for streaming tracks, and the stream queue
later decreases, showing that the mixer consumes input.

However, the global limit of 16 post-mix records was exhausted by zero PCM callbacks before
DirectSound streaming playback began. The current log therefore cannot determine whether JAM
PCM reached the final mixer output. Post-mix instrumentation must collect a small number of
initial silent callbacks separately from the first non-zero PCM callbacks.

## 2026-09-14 트랙별 cooked PCM 계측

보완된 실행 로그에서는 초기 효과음에서 post-mix peak가 약 `0.6`으로 기록되었다.
따라서 SDL playback device와 mixer 합산 출력 자체는 동작한다. 반면 전역 post-mix
샘플 제한이 효과음에서 먼저 소진되므로 JAM streaming track의 PCM 도달 여부는 여전히
분리되지 않는다.

streaming voice에만 SDL_mixer track cooked callback을 연결하고, 해당 track이 mixer에
전달한 변환 후 PCM의 peak/RMS를 bounded trace로 기록한다. 이 값이 0이면 stream 입력
또는 track 변환 경계를 추가 조사하고, non-zero이면 JAM 데이터가 mixer까지 도달한 뒤의
출력 장치·볼륨·게임별 재생 제어를 조사한다.

## 2026-09-14 Per-track cooked-PCM instrumentation

The improved run records a post-mix peak of about `0.6` for an initial sound effect.
Therefore the SDL playback device and mixer aggregate output are working. The global
post-mix sample limit is consumed by effects before the JAM stream, so it still does not
isolate whether the JAM streaming track reaches the mixer.

A cooked callback is now attached to each voice and armed only for streaming playback.
The callback records bounded peak/RMS values for the transformed PCM of that specific track.
A zero value keeps the investigation at the stream-input or track-conversion boundary;
non-zero values move it to output-device, volume, or game-specific playback control after
the JAM data has reached the mixer.

### User reproduction procedure

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe ez2d2m --audio-volume-trace --io-config .\config\ez2dancer-io.example.ini
```

Select `JAM`, let the track run for at least ten seconds after it starts, and close the
game. Compare the run's `*.vfs.log` and same-named `*.audio.log` under
`logs\\windows_x86_launcher_probe\\ez2d2m`. Original audio samples are not written to logs.
## 2026-09-13 complete trace 이후 SDL 출력 경계 확인

### 확인됨

완전 진단 실행 "20260913-232223-242"에서 JAM의 "jam.ezw" open/read와 DirectSound
streaming buffer 공급까지는 확인되었습니다. 그러나 현재 audio trace는
SDL_PutAudioStreamData 이후 SDL_mixer output callback이 실제로 호출되었는지, 그리고
실제 playback device가 열렸는지를 기록하지 않습니다.

현재 코드의 Sdl3MixerAudioBackend::Play()는 streaming track에
MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false를 설정하지 않습니다. SDL_mixer의
기본값은 true이므로 input stream queue가 일시적으로 고갈되면 track이 stopped 상태로
바뀌고, 이후 Unlock()이 새 PCM을 추가해도 자동 재생이 재개되지 않을 수 있습니다.
기존 설계의 "공급 지연 시 silence를 내고 다음 Unlock에서 계속한다"는 계약과 구현이
불일치합니다.

또한 backend 생성자는 MIX_CreateMixerDevice() 실패 시 MIX_CreateMixer() memory-only
mixer로 fallback합니다. has_playback_device_는 false로 남지만 DirectSoundCreate는 이
상태를 진단 메시지만 남기고 계속 성공시킵니다. 이 경로에서는 queue와 PCM trace가
정상이어도 실제 장치로 전송되지 않습니다.

### 판단

현재 JAM 무음의 원인을 하나로 확정할 수는 없습니다. 다만 파일 read 이후의 두 가지
HLE 결함 후보가 확인되었습니다.

- **확인됨:** streaming track의 exhaustion 정책이 설계 계약과 다릅니다.
- **확인됨:** playback device가 없는 memory-only fallback이 제품 실행에서 거부되지 않습니다.
- **미확정:** 해당 JAM 실행이 memory-only fallback을 사용했는지, 또는 queue exhaustion으로
  track이 중지되었는지는 현재 로그에 device 상태와 output callback 관측이 없어 확인할
  수 없습니다.

다음 구현 작업에서는 output device 선택 결과, mixer output format, stream queued bytes,
track playing/stopped 상태와 post-mix PCM peak/RMS를 bounded trace로 기록하고,
streaming Play에는 exhaustion false 정책을 적용해야 합니다. 실제 playback device가
없을 때는 원인을 숨기지 않도록 DirectSoundCreate 실패 또는 명시적인 headless 상태를
선택해야 합니다.

## English

## Checking the SDL output boundary after the 2026-09-13 complete trace

### Confirmed

The complete run "20260913-232223-242" confirms JAM "jam.ezw" open/read and delivery
into a DirectSound streaming buffer. The current audio trace does not show whether the
SDL_mixer output callback consumed the data after SDL_PutAudioStreamData, nor whether a
real playback device was opened.

Sdl3MixerAudioBackend::Play() does not set
MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false for streaming tracks. SDL_mixer defaults
this property to true; if the input stream queue becomes empty even briefly, the track can
transition to stopped and later Unlock calls that append PCM will not automatically restart
it. This conflicts with the existing design contract that a temporary supply delay should
produce silence and continue on a later Unlock.

The backend constructor also falls back from MIX_CreateMixerDevice() to MIX_CreateMixer(),
which is a memory-only mixer. has_playback_device_ remains false, but DirectSoundCreate
only emits a diagnostic message and continues successfully. In this path, queue and PCM
traces can look healthy while no data reaches a physical playback device.

### Assessment

The current evidence does not prove one single cause for JAM silence. Two post-read HLE
candidates are confirmed:

- **Confirmed:** the streaming track exhaustion policy does not match the design contract.
- **Confirmed:** the product accepts a memory-only fallback when no playback device exists.
- **Unresolved:** whether this JAM run used the memory-only fallback or whether queue
  exhaustion stopped the track, because device state and output-callback observations are
  absent from the current trace.

The next implementation task should add bounded diagnostics for device selection, mixer
output format, stream queued bytes, track playing/stopped state, and post-mix PCM peak/RMS,
then set the streaming exhaustion policy to false. If no playback device exists, the product
should fail DirectSoundCreate or expose an explicit headless state instead of hiding the
condition.
