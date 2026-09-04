# 20260905-189 EZ2DJ 4th DirectSound HLE 연동 및 모드 선택 크래시 진단 작업 지시서
# 20260905-189 EZ2DJ 4th DirectSound HLE Integration & ModeSelect Crash Diagnostics Work Order

## 1. 목표 (Goal)

EZ2DJ 4th Trax의 `hle_directsound`를 활성화하고, 언팩된 IAT 슬롯 `0x006d1664` 및 동적 해석기에 `Re2djHleDirectSoundCreate`를 연결하여 `AdvSound = 1` 상태에서 발생하는 `0xc0000094` (Divide By Zero) 예외를 진단하고 해결한다.

Activate `hle_directsound` in the `ez2dj4th` profile, wire `Re2djHleDirectSoundCreate` into the unpacked IAT slot `0x006d1664` and the dynamic resolver, and diagnose and resolve the `0xc0000094` (Divide By Zero) exception occurring under `AdvSound = 1`.

---

## 2. 작업 절차 (Steps)

1. `src/target/target_profile.cpp`:
   - `ez2dj4th` 프로필 `run_defaults`에 `hle_directsound = true;` 설정.
2. `src/tools/windows_x86_launcher_probe/main.cpp`:
   - `target->id == "ez2dj4th"`인 경우 정적 `FindIatSlotByOrdinal("DSOUND.dll", 1)` 부재를 허용.
   - 언팩 완료 시점(`entry_restored`)에서 `0x006d1664` 슬롯을 `_Re2djHleDirectSoundCreate@12`로 패치.
3. `src/platform/windows/injected_runtime.cpp`:
   - `Re2djHleGetProcAddress`에서 `DSOUND.dll`의 `DirectSoundCreate` 및 Ordinal 1을 `Re2djHleDirectSoundCreate`로 라우팅.
4. `src/platform/windows/directsound_com_facade.cpp`:
   - `is_streaming()` 판정 로직을 확장하여 `DSBCAPS_GETCURRENTPOSITION2` 및 크기 360,448 버퍼를 스트리밍으로 인식.
   - `is_duplicate_` 플래그를 두어 복제된 버퍼가 스트리밍으로 오인되지 않도록 보장.
5. `src/platform/windows/audio_volume_trace.cpp`:
   - 트레이스 로그 경로의 상위 디렉터리 생성 및 `OutputDebugStringA` 지원 보강.
6. 빌드 및 테스트:
   - `cmd /c scripts\build_win32.bat`
   - `re2dj_unit_tests.exe`
   - `re2dj_windows_vfs_runtime_probe.exe`
   - `ez2dj4th` 실행 및 진단 로그와 오디오 스트리밍 청취 확인.

