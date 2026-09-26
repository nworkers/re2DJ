# 작업 383 설계 — DirectSound 진입과 창 조회 / Task 383 design — DirectSound entry and window queries

선행: [작업 382 설계](20260926-382-directx-device.md)

## 배경 / Background

작업 382 뒤 Linux 실행은 장치 설정과 글꼴 읽기를 마치고 `user32!GetForegroundWindow`에서 멈췄다. 그 뒤로는 다음 경계가 차례로 이어진다.

1. `dsound.dll!DirectSoundCreate`(ordinal 1)
2. `IDirectSound::SetCooperativeLevel`, 그리고 primary 버퍼를 만드는 `CreateSoundBuffer`
3. `user32!GetWindowLongA(hwnd, GWL_HINSTANCE)`

Windows 실행 기록에 따르면 게임은 이어서 secondary 버퍼를 여러 개 만든다. 각 버퍼는 `DuplicateSoundBuffer`로 세 번 복제한다.

Windows facade(`directsound_com_facade.cpp`)의 규칙은 다음과 같다.

- **`DirectSoundCreate`**: 출력 포인터가 없으면 `DSERR_INVALIDPARAM`, aggregation이면 `DSERR_NOAGGREGATION`이다. device GUID는 보지 않는다.
- **`CreateSoundBuffer`**:
  - `dwSize`가 `DSBUFFERDESC1`보다 작으면 `DSERR_INVALIDPARAM`이다.
  - primary는 게임이 무엇을 요청하든 48 kHz 16-bit stereo 4096바이트다.
  - secondary는 PCM이고 크기가 1바이트–64 MiB여야 한다. 아니면 `DSERR_BADFORMAT`이다.
- **caps**: device는 primary stereo·16-bit이고, 버퍼는 자기 flags와 크기다.
- **복제**: primary는 복제할 수 없고(`DSERR_INVALIDCALL`), 복제본은 원본과 sample을 공유한다.
- **`Lock`**: offset부터 잡고 끝을 넘으면 처음으로 돌아간다. `DSBLOCK_ENTIREBUFFER`는 버퍼 전체를 잡는다.

*After Task 382 a Linux run finished the device setup and font reads and stopped at `user32!GetForegroundWindow`. These boundaries come next, in order:*

1. *`dsound.dll!DirectSoundCreate` (ordinal 1)*
2. *`IDirectSound::SetCooperativeLevel`, then `CreateSoundBuffer` for the primary buffer*
3. *`user32!GetWindowLongA(hwnd, GWL_HINSTANCE)`*

*The Windows run's record shows the game going on to make several secondary buffers, duplicating each one three times with `DuplicateSoundBuffer`.*

*The Windows facade (`directsound_com_facade.cpp`) has these rules:*

- ***`DirectSoundCreate`:** no out pointer is `DSERR_INVALIDPARAM`, aggregation is `DSERR_NOAGGREGATION`, and the device GUID is ignored.*
- ***`CreateSoundBuffer`:***
  - *a `dwSize` below `DSBUFFERDESC1`'s is `DSERR_INVALIDPARAM`;*
  - *the primary is 4096 bytes of 48 kHz 16-bit stereo, whatever the game asks for;*
  - *a secondary must be PCM and 1 byte to 64 MiB, else `DSERR_BADFORMAT`.*
- ***Caps:** the device reports primary stereo and 16-bit; a buffer reports its own flags and size.*
- ***Duplication:** the primary cannot be duplicated (`DSERR_INVALIDCALL`), and a duplicate shares its source's samples.*
- ***`Lock`:** takes from the offset and wraps to the start; `DSBLOCK_ENTIREBUFFER` takes the whole buffer.*

## 결정 / Decisions

1. **DirectSound core(`re2dj::audio`, `directsound_abi.h`·`directsound_device.h`).** 기존 버퍼 정책(`directsound_buffer_policy.h`)과 같은 곳에 둔다.
   - ABI: `WAVEFORMATEX`(18바이트, packed), `DSBUFFERDESC`, `DSCAPS`, `DSBCAPS`, HRESULT, flag, IID.
   - 규칙: `PlanSoundBuffer`, `DeviceCaps`, `BufferCaps`, `CheckDuplicate`, `PlanLock`.
   - Windows의 `LegacyAudioBuffer::Lock`도 `PlanLock`을 쓴다.

   ***DirectSound core (`re2dj::audio`, `directsound_abi.h` and `directsound_device.h`),** beside the existing buffer policy (`directsound_buffer_policy.h`):*
   - *ABI: `WAVEFORMATEX` (18 bytes, packed), `DSBUFFERDESC`, `DSCAPS`, `DSBCAPS`, HRESULTs, flags, and IIDs.*
   - *Rules: `PlanSoundBuffer`, `DeviceCaps`, `BufferCaps`, `CheckDuplicate`, and `PlanLock`.*
   - *Windows' `LegacyAudioBuffer::Lock` uses `PlanLock` too.*
2. **Windows.** 버퍼 생성, device와 버퍼의 caps, 복제 거절, speaker 설정이 core를 쓴다. SDK와의 일치는 `static_assert`로 확인한다. 이 facade는 DirectSound 3 헤더를 쓰므로 `DSBUFFERDESC`가 20바이트다.
   ***Windows:** buffer creation, the device and buffer caps, the duplicate refusal, and the speaker configuration use the core, checked against the SDK with `static_assert`. The facade builds against the DirectSound 3 headers, so its `DSBUFFERDESC` is 20 bytes.*
3. **Linux `dsound.dll`(`dsound_module.cpp`).** resolve-only 목록에서 실제 module로 옮긴다. `DirectSoundCreate`는 ordinal 1이다.
   - `IDirectSound` 11개 메서드를 모두 구현한다. `SetCooperativeLevel`·`Compact`·`SetSpeakerConfig`는 `DS_OK`, `Initialize`는 `DSERR_ALREADYINITIALIZED`다.
   - `IDirectSoundBuffer`에서 구현하는 메서드는 `QueryInterface`, `AddRef`, `Release`, `GetCaps`, `GetFormat`, `Initialize`, `Lock`, `Unlock`, `Restore`다. 재생과 제어 메서드는 불리면 멈춘다.
   - sample은 guest `VirtualAlloc` 메모리에 두고 0으로 채운다. `Lock`은 guest 주소를 돌려준다.
   - 복제본은 sample을 공유한다. 마지막으로 해제되는 버퍼가 메모리를 돌려준다.
   - Linux에는 아직 소리 출력이 없다. 재생은 이후 단계에서 공용 SDL3 backend로 더한다.

   ***Linux `dsound.dll` (`dsound_module.cpp`)** moves from the resolve-only list to a real module, with `DirectSoundCreate` as ordinal 1.*
   - *All 11 `IDirectSound` methods: `SetCooperativeLevel`, `Compact`, and `SetSpeakerConfig` answer `DS_OK`, and `Initialize` answers `DSERR_ALREADYINITIALIZED`.*
   - *`IDirectSoundBuffer` implements `QueryInterface`, `AddRef`, `Release`, `GetCaps`, `GetFormat`, `Initialize`, `Lock`, `Unlock`, and `Restore`; the playback and control methods stop when called.*
   - *Samples are zero-filled guest `VirtualAlloc` memory, and `Lock` returns guest addresses.*
   - *Duplicates share the samples, and the last buffer released returns the memory.*
   - *Linux has no sound output yet; playback comes in a later phase through the shared SDL3 backend.*
4. **user32.**
   - `GetForegroundWindow`은 guest의 한 스레드가 가진 활성 창을 돌려준다. guest가 host의 foreground 앱이기 때문이다.
   - `GetWindowLongA`는 Windows 11에서 측정한 대로 동작한다. `GWL_WNDPROC`·`HINSTANCE`·`HWNDPARENT`·`ID`·`STYLE`·`EXSTYLE`·`USERDATA`는 창의 필드를 읽고, 0 이상의 index는 cbWndExtra의 DWORD를 읽는다.
     - 성공하면 last error를 바꾸지 않는다.
     - 모르는 index는 0을 돌려주고 `ERROR_INVALID_INDEX`(1413)를 설정한다.
     - 모르는 창은 0을 돌려주고 `ERROR_INVALID_WINDOW_HANDLE`(1400)을 설정한다.

   ***user32:***
   - *`GetForegroundWindow` returns the active window of the guest's one thread, since the guest is the host's foreground application.*
   - *`GetWindowLongA` works as measured on Windows 11. `GWL_WNDPROC`, `HINSTANCE`, `HWNDPARENT`, `ID`, `STYLE`, `EXSTYLE`, and `USERDATA` read the window's fields, and a non-negative index reads a DWORD of cbWndExtra.*
     - *Success leaves the last error alone.*
     - *An unknown index is 0 with `ERROR_INVALID_INDEX` (1413).*
     - *An unknown window is 0 with `ERROR_INVALID_WINDOW_HANDLE` (1400).*

## 범위 밖 / Out of scope

- 버퍼의 재생과 제어(`Play`, `Stop`, 위치, 볼륨, pan, 주파수, 상태)와 Linux 소리 출력. / *Buffer playback and controls (`Play`, `Stop`, position, volume, pan, frequency, status) and Linux sound output.*
- DirectInput(`DirectInputCreateA`): 다음 작업. / *DirectInput (`DirectInputCreateA`): the next task.*
