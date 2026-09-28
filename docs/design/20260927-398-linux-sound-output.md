# 작업 398 설계 — Linux 소리 출력 / Task 398 design — Linux sound output

선행: [작업 383 설계](20260926-383-directsound-entry.md), [작업 387 설계](20260926-387-directsound-controls.md)

## 배경 / Background

사용자가 Linux x64에서 소리가 나지 않는다고 알렸다. 작업 387의 Linux `dsound.dll`은 무음 재생만 한다. 제어 규칙은 Windows와 같지만, 재생 커서는 시계로 계산하고 샘플을 host 장치로 내보내지 않는다. Linux 빌드는 SDL 오디오도 꺼져 있었다(`SDL_AUDIO`는 Windows x86에서만 켬).

Windows 제품은 공용 `Sdl3MixerAudioBackend`(SDL3_mixer)로 소리를 낸다. 버퍼마다 voice 하나가 host 메모리의 `LegacyAudioBuffer`를 재생하고, facade가 정해진 순서로 backend를 부른다.

*A user reported no sound on Linux x64. Task 387's Linux `dsound.dll` only plays silently: its control rules are Windows', but its play cursor follows the clock and no samples reach a host device. Linux builds also had SDL audio off (`SDL_AUDIO` was on for Windows x86 only).*

*The Windows product plays through the shared `Sdl3MixerAudioBackend` (SDL3_mixer): one voice per buffer plays an `LegacyAudioBuffer` in host memory, and the facade calls the backend in a set order.*

## 결정 / Decisions

1. **host 오디오 계약.** `hle::HostAudio`를 둔다. `ImportCallServices::Audio()`로 받고, voice 단위 호출은 Windows facade가 backend를 부르는 것과 같다: `CreateVoice`, `DestroyVoice`, `Play`, `CommitStreamingWrite`, `Stop`, `SetPosition`, `UpdateControls`, `PositionBytes`, `IsPlaying`. host가 없으면 지금의 무음 재생 그대로다.
   ***The host audio contract.** `hle::HostAudio`, reached through `ImportCallServices::Audio()`, with per-voice calls matching how the Windows facade calls its backend: `CreateVoice`, `DestroyVoice`, `Play`, `CommitStreamingWrite`, `Stop`, `SetPosition`, `UpdateControls`, `PositionBytes`, `IsPlaying`. With no host, buffers play silently as before.*
2. **Linux `dsound.dll`.** host가 있으면 버퍼마다 다음을 둔다.
   - voice와 host 사본(`LegacyAudioBuffer`). 샘플은 여전히 guest 메모리에 있어서 guest의 lock이 바로 그 메모리를 쓴다.
   - Unlock 때 lock한 구역을 사본으로 복사하고, 스트리밍 버퍼는 commit한다. Windows facade가 unlock에서 commit하는 것과 같다.
   - Play, Stop, SetCurrentPosition, GetCurrentPosition, GetStatus, 볼륨·팬·주파수는 Windows facade와 같은 순서로 voice에 넘긴다. Stop은 voice가 도달한 위치를 저장한다. 재생 커서를 묻지 않은 `GetCurrentPosition`의 쓰기 커서는 버퍼 자신의 위치다.
   - 스트리밍 판정은 공용 `IsStreamingBufferDescription`이다. 주 버퍼와 복제본은 스트리밍이 아니다.
   - 복제본은 원본 사본의 샘플을 공유한다(`Duplicate`).
   - host가 voice를 주지 못하면 `DSERR_NODRIVER`다. Windows facade와 같다.
   - 버퍼가 사라지면 voice도 돌려준다. host는 guest process보다 오래 산다.

   ***Linux `dsound.dll`.** With a host, each buffer has:*
   - *A voice and a host copy (`LegacyAudioBuffer`). Samples still live in guest memory, so the guest's locks write it directly.*
   - *At Unlock the locked regions are copied into the host copy, and a streaming buffer commits them, as the Windows facade commits at unlock.*
   - *Play, Stop, SetCurrentPosition, GetCurrentPosition, GetStatus, and volume, pan and frequency reach the voice in the Windows facade's order. Stop keeps the position the voice had reached, and a `GetCurrentPosition` that does not ask for the play cursor gets the buffer's own position as the write cursor.*
   - *Streaming is decided by the shared `IsStreamingBufferDescription`; primaries and duplicates never stream.*
   - *A duplicate shares the original copy's samples (`Duplicate`).*
   - *A host with no voice to give fails creation with `DSERR_NODRIVER`, as the Windows facade does.*
   - *A released buffer returns its voice; the host outlives the guest process.*
3. **Linux host.** `LinuxHostAudio`는 Windows와 같은 `Sdl3MixerAudioBackend`를 기본 재생 장치로 연다. WSLg에서는 PulseAudio다.
   - master gain은 Windows와 같이 `--audio-gain-db`나 profile 값(4th는 0dB)에서 온다. `--audio-gain-db`는 이제 Linux에서도 받는다.
   - 재생 장치가 없으면 backend가 들리지 않는 mixer로 계속 돌고, 로그에 이유를 적는다.

   ***The Linux host.** `LinuxHostAudio` opens the same `Sdl3MixerAudioBackend` as Windows on the default playback device, PulseAudio under WSLg.*
   - *Its master gain comes from `--audio-gain-db` or the profile (0 dB for the 4th), as on Windows; `--audio-gain-db` is now accepted on Linux.*
   - *With no playback device the backend keeps mixing unheard, and the log says why.*
4. **빌드.** Linux에서도 `SDL_AUDIO`와 SDL3_mixer를 켠다. SDL은 ALSA와 PulseAudio를 동적으로 연다. 64비트·32비트 개발 패키지가 모두 필요하다. backend 스트리밍 테스트(`re2dj_audio_stream_progress_test`, dummy 장치)도 Linux에서 돈다.
   ***Build.** Linux enables `SDL_AUDIO` and SDL3_mixer too; SDL opens ALSA and PulseAudio dynamically, needing the 64- and 32-bit development packages. The backend streaming test (`re2dj_audio_stream_progress_test`, on the dummy device) now runs on Linux as well.*

## 범위 밖 / Out of scope

- Windows의 오디오 진단 trace와 볼륨 추적(`--audio-volume-trace`). / *Windows' audio diagnostic traces and volume tracing (`--audio-volume-trace`).*
- `--demo-volume`. / *`--demo-volume`.*
- 3D 소리, 효과, 캡처. / *3D sound, effects, and capture.*
