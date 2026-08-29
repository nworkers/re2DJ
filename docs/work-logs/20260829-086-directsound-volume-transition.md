# DirectSound 데모 음량 설정 HLE 작업 로그

관련 설계: [DirectSound 데모 음량 설정 HLE](../design/20260829-086-directsound-volume-transition.md)

관련 작업 지시: [DirectSound 데모 음량 설정 HLE 작업 지시](../work-orders/20260829-086-directsound-volume-transition.md)

관련 분석: [EZ2DJ 데모 음량 프로필 분석](../analysis/ez2dj-demo-volume.md)

## 한국어

### 원인 분석

사용자 재검증 뒤 약 60초의 `20260829-001324-200.audio.log`를 분석했다. streaming queue는 bounded 상태였고 master `+6 dB`도 적용됐지만 title buffer는 `SetVolume(-10000)`을 한 번만 받았다. caller attribution은 wrapper 밖 원본 RVA `0x3120f`를 확인했다.

대응 unprotected binary를 읽기 전용으로 분석한 결과 이 함수는 전역 인덱스로 `[-10000, -2222, -1111, 0]` table을 조회했다. 인덱스는 `GetPrivateProfileIntA("GAMEASSIGNMENTS", "DemoVolume", 3, ...)`에서 왔으며 사용자 HDD의 실제 INI는 `DemoVolume=0`이었다. 따라서 streaming이나 PCM 변환이 아니라 원본 설정이 음소거 profile을 선택한 것이 직접 원인이다.

### 구현

- `ini_profile_hle.*`에 target-specific profile thunk를 분리했다.
- `GAMEASSIGNMENTS/DemoVolume`만 외부 profile `0..3`으로 재정의하고 다른 요청은 Win32 API로 pass-through한다.
- launcher는 runtime export와 main-image `GetPrivateProfileIntA` IAT slot을 준비·검증한다.
- 제품 CLI에 `--demo-volume <0..3>`을 추가하고 기본값을 3으로 설정했다.
- master gain 기본값을 `+6 dB`에서 `0 dB`로 변경했다.
- `SetVolume` caller RVA와 실제 track/master gain의 bounded 진단을 유지했다.
- runtime/product probe에 override, pass-through, 기본값과 범위 검증을 추가했다.

원본 EXE, HDD INI와 PCM 자산은 변경하거나 저장소에 포함하지 않았다.

### 검증

- 기존 `build/windows-x86`의 compile 단계는 통과했지만 다른 보호 세션의 잔존 `ez2dj` PID 6088이 기존 runtime DLL을 잠가 link가 거부됐다. 관리자 종료도 access denied였다.
- 기존에 받은 SDL/SDL_mixer source를 재사용한 별도 `build/windows-x86-task086`에서 Windows x86 전체 build를 통과했다.
- CTest 3/3 통과:
  - `re2dj_windows_vfs_runtime_probe`
  - `re2dj_windows_product_loader_probe`
  - `re2dj_unit_tests`
- debugger mode 실제 실행 `20260829-003716-488.audio.log`에서 다음을 확인했다.
  - `ini:demo-volume configured=3`
  - `SetVolume` original caller RVA `0x3120f`, requested/applied `0`
  - track gain `1.0`, master gain `1.0`
  - 45,056바이트 dirty 청크의 `backend-refresh=1`과 약 312–315KB bounded queue

debugger mode 검증 자식은 종료됐고 원본 HDD는 읽기 전용으로 유지됐다. 전체 곡과 gameplay 효과음의 최종 청취 평가는 사용자 재검증 항목으로 남긴다.

## English

### Cause analysis

After user revalidation, the roughly 60-second `20260829-001324-200.audio.log` showed a bounded streaming queue and applied +6 dB master gain, but only one persistent `SetVolume(-10000)` on the title buffer. Caller attribution identified original RVA `0x3120f` outside the wrapper.

Read-only analysis of the corresponding unprotected binary showed that this function indexes `[-10000, -2222, -1111, 0]` with a global loaded from `GetPrivateProfileIntA("GAMEASSIGNMENTS", "DemoVolume", 3, ...)`. The user HDD's actual INI contains `DemoVolume=0`. The direct cause is therefore the original setting selecting its mute profile, not streaming or PCM conversion.

### Implementation

Added a separate target-specific profile thunk in `ini_profile_hle.*`. It overrides only `GAMEASSIGNMENTS/DemoVolume` with external profile `0..3` and passes all other calls through to Win32. The launcher configures the runtime export and verifies the main-image IAT slot. Product CLI now exposes `--demo-volume <0..3>` with default 3, while master gain defaults back to 0 dB. Bounded caller-RVA and actual gain tracing remains available. Runtime and product probes cover override, pass-through, defaults, and range validation. No original EXE, HDD INI, or PCM asset was modified or added to the repository.

### Verification

The normal build compiled but could not relink its existing runtime DLL because protected-session process PID 6088 retained the file and denied administrator termination. A clean complete Win32 x86 build using the already-downloaded SDL sources passed in `build/windows-x86-task086`. CTest passed 3/3. Real debugger-mode trace `20260829-003716-488.audio.log` confirms configured profile 3, original caller RVA `0x3120f`, requested/applied volume 0, track/master gain 1.0, and continuing 45,056-byte dirty refreshes with a bounded roughly 312–315 KB queue. The debugger-mode child was terminated, and the original HDD remained read-only. Final listening across complete songs and gameplay effects remains a user-revalidation item.
