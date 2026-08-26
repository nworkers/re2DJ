# DirectSound SDL3/SDL3_mixer HLE 작업 로그

관련 설계: [DirectSound SDL3/SDL3_mixer HLE](../design/20260826-069-directsound-sdl3-mixer-hle.md)
관련 작업 지시: [DirectSound SDL3/SDL3_mixer HLE 작업 지시](../work-orders/20260826-069-directsound-sdl3-mixer-hle.md)

## 결과

- SDL 3.4.14와 SDL_mixer 3.2.4를 commit에 고정하고 zlib license를 고지했다.
- SDL3는 audio subsystem만 활성화하고 SDL_mixer의 선택 codec integration은 모두 껐다.
- 공용 `LegacyAudioBuffer`가 PCM, circular Lock, cursor와 control state를 보존한다.
- Windows x86 facade가 DirectSoundCreate, primary/secondary buffer와 전체 IDirectSoundBuffer vtable을 제공한다.
- launcher의 `--hle-directsound`가 `DSOUND.dll` ordinal `#1` IAT를 교체한다.
- facade probe는 dummy SDL driver에서 create, Lock, Unlock, seek, Play와 Stop을 검증한다.
- 재검증에서 `-DRE2DJ_WARNINGS_AS_ERRORS=ON`일 때 SDL 헤더의 C4819가 `/WX`로 에러가 되어 x86 빌드가 실패하는 것을 발견하고, SDL include 앞뒤 `#pragma warning(push)/(pop)` 범위 억제로 해소했다.

## 원본 실행

`20260826-005558-184.jsonl`은 실제 SDL playback device를 사용했다. primary buffer 1개와 secondary buffer 121개가 생성됐고 Lock과 Unlock은 각각 299회 성공했다. `title.wav`는 parser와 CreateSoundBuffer를 통과하고 360,448바이트 streaming buffer가 `DSBPLAY_LOOPING`으로 SDL_mixer track에 제출됐다. headless fallback과 OpenGL failure는 없었다.

최초 access violation은 execute address `0x00000000`, return `0x00420276`이다. call site `0x00420273`은 global `IDirect3D3` vtable `+0x24`, 즉 `CreateVertexBuffer`를 호출한다. 따라서 이 AV는 DirectSound가 아니라 다음 Direct3D HLE 경계다.

## 검증

- Windows x86 injected runtime/launcher/unit/probe build: 성공
- Windows x86 CTest: 2/2 성공
- Windows x64 common core/unit build와 CTest: 1/1 성공
- 원본 HDD/실행 파일: 읽기 전용 유지
- 실제 audio와 framebuffer의 청취·시각 정확성: 사용자 검증 필요

## 재검증

- `windows-x86` preset을 `-DRE2DJ_WARNINGS_AS_ERRORS=ON`(저장소 표준, `scripts/test_all.ps1`)으로 재구성해 빌드하면 SDL 헤더에서 C4819가 `/WX`로 승격돼 실패했다. `src/audio/sdl3_mixer_audio_backend.cpp`의 SDL include를 `#pragma warning(disable : 4819)` 범위로 감싸 해소했고, 이후 동일 구성 빌드는 경고 0으로 성공했다.
- `windows-x64`도 `-DRE2DJ_WARNINGS_AS_ERRORS=ON`으로 재검증해 경고 0, CTest 1/1 성공을 확인했다(CI windows-x64 job과 동일 조건).
- 라이브 재실행 `logs/windows_x86_launcher_probe/ez2dj1stse/20260826-014926-561.jsonl`은 기준 trace `20260826-005558-184.jsonl`과 결정적으로 동일하다. primary 1개, secondary 121개(`0x140e2` 119 + `0x140c6` 2), Lock/Unlock 각 299회, looping Play 1회, OpenGL failure 0회.
- null `CreateVertexBuffer` AV가 guest에 전달된 직후 같은 thread·같은 ESP의 두 번째 `c0000005`(address `0xfaa77401`, 미커밋 region)로 process가 종료되는 후속 붕괴도 두 실행에서 동일하게 나타났다. 이는 audio HLE의 실패가 아니라 처리되지 않은 다음 graphics 경계의 dispatch 붕괴이며, 세부는 analysis 항목 28에 기록했다.

## Re-verification (English)

- Configuring the `windows-x86` preset with `-DRE2DJ_WARNINGS_AS_ERRORS=ON` (the repository standard used by `scripts/test_all.ps1`) failed the build because C4819 from SDL headers is promoted by `/WX`. Wrapping the SDL includes in `src/audio/sdl3_mixer_audio_backend.cpp` with a scoped `#pragma warning(disable : 4819)` resolves it; the same configuration now rebuilds with zero warnings.
- The `windows-x64` build was re-verified under `-DRE2DJ_WARNINGS_AS_ERRORS=ON` with zero warnings and a passing CTest run (matching the CI windows-x64 job).
- Live rerun `logs/windows_x86_launcher_probe/ez2dj1stse/20260826-014926-561.jsonl` matches baseline trace `20260826-005558-184.jsonl` deterministically: one primary buffer, 121 secondary buffers (119 static `0x140e2` plus 2 streaming `0x140c6`), 299 Lock/Unlock pairs, one looping Play, and zero OpenGL failures.
- After the null CreateVertexBuffer AV reaches the guest, both runs collapse identically through a second c0000005 at address 0xfaa77401 on an uncommitted region with the same thread and ESP, and the process exits 0xc0000005. This is post-dispatch collapse behind the unhandled next graphics boundary rather than an audio HLE failure; details are recorded as analysis item 28.

---

# DirectSound SDL3/SDL3_mixer HLE Work Log

SDL 3.4.14 and SDL_mixer 3.2.4 are pinned and recorded under their zlib license. Only SDL audio and SDL_mixer raw PCM support are required. A neutral LegacyAudioBuffer owns PCM, circular locks, cursors, and control state; the Windows x86 facade supplies DirectSound and DirectSoundBuffer COM ABI; and `--hle-directsound` replaces DSOUND ordinal 1.

Trace 20260826-005558-184.jsonl uses a real SDL playback device and records one primary buffer, 121 secondary buffers, 299 successful Lock/Unlock pairs, and looping submission of a 360,448-byte title.wav streaming buffer. No headless fallback or OpenGL failure occurs. The next execute AV at address zero returns to 0x00420276 and comes from global IDirect3D3 vtable slot +0x24 CreateVertexBuffer at call site 0x00420273, making it the next graphics HLE boundary rather than an audio failure.

Windows x86 builds and both tests pass, as do the Windows x64 shared-core build and unit test. Original assets remain read-only; audible and visual accuracy still require user observation.
