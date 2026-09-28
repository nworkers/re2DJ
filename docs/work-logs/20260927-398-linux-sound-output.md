# 작업 398 작업 로그 — Linux 소리 출력 / Task 398 work log — Linux sound output

설계: [20260927-398-linux-sound-output.md](../design/20260927-398-linux-sound-output.md)
작업 지시서: [20260927-398-linux-sound-output.md](../work-orders/20260927-398-linux-sound-output.md)

## 진행 / Progress

WSL(Ubuntu 24.04)의 오디오 환경은 다음과 같았다.

- `PULSE_SERVER=unix:/mnt/wslg/PulseServer`
- `libpulse-dev`와 `libasound2-dev`가 amd64·i386 모두 설치됨
- pipewire 개발 패키지는 없음

SDL 구성 결과는 `Audio drivers: alsa(dynamic) disk dummy pulseaudio(dynamic)`였다.

*The WSL (Ubuntu 24.04) audio environment:*

- *`PULSE_SERVER=unix:/mnt/wslg/PulseServer`*
- *`libpulse-dev` and `libasound2-dev` installed for both amd64 and i386*
- *no pipewire development package*

*SDL configured with `Audio drivers: alsa(dynamic) disk dummy pulseaudio(dynamic)`.*

실제 실행에서 두 폭 모두 다음 로그를 남겼다. 재생 장치를 열었다는 뜻이다(없으면 장치 없음 사유가 붙는다).

`host audio : SDL3_mixer, master gain 0.0 dB`

x64 실행의 API 기록에서 `IDirectSoundBuffer::GetCurrentPosition`이 0, 0x0C00, 0x1000, 0x1C00으로 늘었다. 재생 커서가 시계 계산이 아니라 SDL3_mixer 트랙의 진행을 따른다.

*In real runs both widths logged the line above, meaning the playback device opened (a missing device would add its reason). In the x64 run's API log `IDirectSoundBuffer::GetCurrentPosition` went 0, 0x0C00, 0x1000, 0x1C00: the play cursor follows the SDL3_mixer track rather than the clock.*

WSL에 `pactl`이 없어서 PulseAudio 쪽 스트림은 직접 확인하지 못했다. 실제로 들리는지는 사용자가 확인해야 한다.

*WSL has no `pactl`, so the stream on the PulseAudio side was not inspected directly; whether it is audible is for the user to confirm.*

Windows 비교의 오디오 기록은 스트리밍 버퍼의 lock/unlock 횟수(153 대 155)만 다르다. 30초 동안 BGM 링을 몇 번 다시 채웠느냐는 시간에 따라 달라진다. 버퍼 생성, 복제, 재생, 정지, 볼륨 기록은 같다.

*In the Windows comparison the audio logs differ only in the streaming buffer's lock/unlock count (153 against 155); how often the BGM ring is refilled in 30 seconds depends on timing. Buffer creation, duplication, play, stop, and volume lines match.*

## 변경 / Changes

- **HLE**: `hle::HostAudio`, `ImportCallServices::Audio()`(기록 서비스가 전달한다). / *`hle::HostAudio` and `ImportCallServices::Audio()` (forwarded by the recording services).*
- **Linux dsound.dll**: 버퍼의 voice·host 사본, 스트리밍 판정, unlock 복사·commit, 제어의 voice 전달, 복제 공유, `DSERR_NODRIVER`, 해제 때 voice 반환. / *Buffer voices and host copies, the streaming decision, unlock copies and commits, controls through the voice, shared duplicates, `DSERR_NODRIVER`, and voices returned on release.*
- **Linux**:
  - `LinuxHostAudio`(`Sdl3MixerAudioBackend` 사용).
  - `OriginalRunEnvironment::audio`.
  - CLI가 host 오디오를 만들고, `--audio-gain-db`를 받는다.

  ***Linux:***
  - *`LinuxHostAudio` (on `Sdl3MixerAudioBackend`).*
  - *`OriginalRunEnvironment::audio`.*
  - *The CLI makes the host audio and accepts `--audio-gain-db`.*
- **빌드 / build**:
  - `RE2DJ_SDL3_AUDIO_ACTIVE`(Windows x86 또는 Linux). SDL3_mixer 가져오기를 공용으로 옮겼다.
  - Linux backend에 오디오 소스를 넣고, `RE2DJ_LINUX_HOST_AUDIO`를 정의한다.
  - Linux에서도 backend 스트리밍 테스트를 돌린다.

  ***Build:***
  - *`RE2DJ_SDL3_AUDIO_ACTIVE` (Windows x86 or Linux), with fetching SDL3_mixer moved to a shared block.*
  - *The Linux backend gets the audio sources and defines `RE2DJ_LINUX_HOST_AUDIO`.*
  - *The backend streaming test runs on Linux too.*
- **테스트 / tests**: dsound 가짜 host 오디오 사례.
  - 스트리밍 commit과 복사된 샘플.
  - 제어(클램프 포함), 위치(되감기), 재생, 두 커서, 상태, 정지 위치.
  - 정적 버퍼와 복제본의 통째 재생과 샘플 공유.
  - 해제 때 voice 반환, `DSERR_NODRIVER`.
  - `MemoryServices::audio`.

  ***Tests:** the dsound fake host audio case:*
  - *streaming commits and copied samples*
  - *controls (with clamping), position (wrapped), play, both cursors, status, and the stop position*
  - *whole playback and shared samples for a static buffer and its duplicate*
  - *the voice returned on release, and `DSERR_NODRIVER`*
  - *`MemoryServices::audio`*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 configure, build, CTest | exit 0, 6/6 |
| Windows 실제 4th, 기준 `9badf71`와 30초씩 / real 4th vs base `9badf71`, 30 s each | ddraw 1,410줄이 같다. 오디오 기록은 스트리밍 lock 횟수만 다르다(시간 의존) / *the 1,410 ddraw lines match; audio logs differ only in the streaming lock count (timing)* |
| Linux x64·x86 configure, build, CTest | 경고·오류 없음, 각각 4/4(unit checks 4,140, 스트리밍 테스트 포함) / *no warnings or errors, 4/4 each (4,140 unit checks, the streaming test included)* |
| Linux `--call-limit 32768`, 두 폭 / both widths | 호출 32,768번에서 멈춤, hardlock 83, IO 읽기 3,777 / *stops at 32,768 calls; hardlock 83; IO reads 3,777* |
| Linux 실제 재생 / real playback | 두 폭 모두 재생 장치를 열었다. 커서가 장치 진행을 따른다 / *both widths opened the playback device; cursors follow the device* |

## 다음 / Next

사용자의 청취 확인, 그리고 `kernel32!FindFirstFileA`(곡 폴더 목록)다.

*Next: the user's listening check, then `kernel32!FindFirstFileA` (song folder listing).*
