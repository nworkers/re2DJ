# 작업 387 설계 — DirectSound 2단계: 버퍼 제어 / Task 387 design — DirectSound phase 2: buffer controls

선행: [작업 383 설계](20260926-383-directsound-entry.md), [작업 386 설계](20260926-386-winmm-mixer.md)

## 배경 / Background

작업 386 뒤 Linux 실행은 `IDirectSoundBuffer::Stop`에서 멈췄다. 게임은 효과음 버퍼 56개를 만들고, 각각을 복제한다. 그리고 버퍼마다 `Stop`, `SetCurrentPosition(0)`, `SetPan(0)`, `SetVolume(0)`을 부른다.

Windows facade의 규칙은 다음과 같다(`LegacyAudioBuffer`와 facade).

- 위치는 버퍼 크기로 wrap한다.
- 볼륨은 −10000~0, pan은 −10000~10000으로 자른다.
- 주파수 0(`DSBFREQUENCY_ORIGINAL`)은 형식의 원래 rate로 돌린다.
- 상태는 `DSBSTATUS_PLAYING`이고, looping으로 재생하면 `DSBSTATUS_LOOPING`이 더해진다.
- 복제본은 정지 상태로 시작하고, 원본의 위치·볼륨·pan·주파수를 가진다.
- `Stop`은 재생이 도달한 위치에 cursor를 남긴다.

재생 cursor는 host backend(SDL3_mixer)가 실제로 재생한 위치다.

*After Task 386 a Linux run stopped at `IDirectSoundBuffer::Stop`. The game makes 56 sound-effect buffers and duplicates each. For each buffer it calls `Stop`, `SetCurrentPosition(0)`, `SetPan(0)`, and `SetVolume(0)`.*

*The Windows facade's rules (in `LegacyAudioBuffer` and the facade):*

- *Positions wrap at the buffer's size.*
- *Volume is clamped to −10000..0 and pan to −10000..10000.*
- *Frequency 0 (`DSBFREQUENCY_ORIGINAL`) restores the format's own rate.*
- *The status is `DSBSTATUS_PLAYING`, plus `DSBSTATUS_LOOPING` for a looping play.*
- *A duplicate starts stopped with its original's position, volume, pan, and frequency.*
- *`Stop` leaves the cursor where playback reached.*

*The play cursor is where the host backend (SDL3_mixer) has actually played.*

## 결정 / Decisions

1. **core (`directsound_device.h`).** 위 규칙을 core로 옮긴다: `WrapPosition`, `ClampVolume`, `ClampPan`, `ResolveFrequency`, `BufferStatus`. flag와 범위 상수는 SDK와 `static_assert`로 비교한다.
   ***Core (`directsound_device.h`):** the rules above move into the core as `WrapPosition`, `ClampVolume`, `ClampPan`, `ResolveFrequency`, and `BufferStatus`, with the flag and range constants checked against the SDK with `static_assert`.*
2. **무음 재생(`AdvanceSilentPlayback`).** 소리 장치가 없는 host는 재생 위치를 시계로 진행한다. 속도는 버퍼 주파수 × `nBlockAlign` 바이트/초다.
   - 반복 재생은 끝에서 wrap한다.
   - 한 번 재생은 끝에 닿으면 멈추고, cursor는 0을 읽는다. 이 0은 측정한 값이 아니다. Windows backend에서 끝난 track이 위치 0을 돌려주는 것에 맞춘 것이다.

   게임이 재생 위치나 상태를 기다리다 영원히 멈추지 않게 하려는 모델이다. Linux 소리 출력 단계가 오면 실제 재생 위치로 바꾼다.

   ***Silent playback (`AdvanceSilentPlayback`).** A host without a sound device advances the play position by the clock, at the buffer's frequency × `nBlockAlign` bytes a second.*
   - *A looping play wraps at the end.*
   - *A one-shot play stops at its end, and its cursor then reads 0. That 0 was not measured; it follows the Windows backend, where a finished track reports position 0.*

   *This model keeps the game from waiting forever on a play position or status. It gives way to the actual play position when the Linux sound-output phase arrives.*
3. **Windows.** `LegacyAudioBuffer`의 위치·볼륨·pan과 facade의 주파수·상태가 core를 쓴다. 재생 cursor는 계속 backend에서 온다.
   ***Windows:** `LegacyAudioBuffer`'s position, volume, and pan, and the facade's frequency and status, use the core; the play cursor still comes from the backend.*
4. **Linux.** `IDirectSoundBuffer`에 다음 메서드를 더한다.
   - `Play`, `Stop`, `SetCurrentPosition`, `GetCurrentPosition`, `GetStatus`
   - `SetVolume`/`GetVolume`, `SetPan`/`GetPan`, `SetFrequency`/`GetFrequency`, `SetFormat`

   Windows와 같이 write cursor는 play cursor와 같다. 복제본은 원본의 제어값을 복사한다.

   ***Linux:** `IDirectSoundBuffer` gains these methods:*
   - *`Play`, `Stop`, `SetCurrentPosition`, `GetCurrentPosition`, and `GetStatus`*
   - *`SetVolume`/`GetVolume`, `SetPan`/`GetPan`, `SetFrequency`/`GetFrequency`, and `SetFormat`*

   *As on Windows, the write cursor equals the play cursor, and a duplicate copies its original's controls.*
5. **호출 한도.** Linux 진단의 호출 한도를 4,096에서 32,768로 올린다. 게임은 효과음 로딩에만 약 12,000번의 호출을 쓴다. 끝없이 도는 실행은 여전히 이 한도에서 멈춘다.
   ***Call limit:** the Linux diagnostic call limit rises from 4,096 to 32,768. Loading the sound effects alone takes the game about 12,000 calls, and a run that loops without end still stops at the limit.*

## 범위 밖 / Out of scope

- Linux 소리 출력. / *Linux sound output.*
- `user32!GetAsyncKeyState`부터 시작하는 입력 조회: 다음 작업. / *Input queries from `user32!GetAsyncKeyState`: the next task.*
