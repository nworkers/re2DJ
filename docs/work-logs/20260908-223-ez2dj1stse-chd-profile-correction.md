# 작업 로그: ez2dj1stse CHD 프로파일 실행 정책 정정

## 한국어

### 관련 문서

- 설계: [ez2dj1stse CHD 프로파일 실행 정책 정정 설계](../design/20260908-223-ez2dj1stse-chd-profile-correction.md)
- 작업 지시: [ez2dj1stse CHD 프로파일 실행 정책 정정](../work-orders/20260908-223-ez2dj1stse-chd-profile-correction.md)
- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)
- 절차: [Hardlock descriptor ID 추출](../guides/hardlock-descriptor-extraction.md)

### 수행 내용

1. [Hardlock descriptor 추출 절차](../guides/hardlock-descriptor-extraction.md)를 CHD `.protect` 빌드에 적용해 장치와 IOCTL 경계를 관측했습니다.
2. CHD 실행 파일의 packed import directory를 직접 파싱해 launcher가 패치할 수 있는 import 전체 목록을 확정했습니다.
3. 옵션을 하나씩 바꿔 실행하며 각 HLE 경계의 성립 여부와 `hle_wts_active_console`의 영향 여부를 확인했습니다.
4. `target_profile.cpp`의 `ez2dj1stse` 실행 기본값을 관측에 맞게 정정했습니다.
5. target profile unit test와 product loader probe의 인자 계약을 정정된 값에 맞췄습니다.
6. analysis, `ARCHITECTURE.md`, `README.md`를 갱신했습니다.

### 확인된 사실

**Packed import directory.** import data directory는 RVA `0x01aebbd0`으로 `.protect` 안에 있고, 원본 `.idata`(RVA `0x01aba000`)는 파일에 남아 있으나 header가 가리키지 않습니다. 정적으로 보이는 import는 `KERNEL32.dll`의 15개 항목(`GetCommandLineA`, `CreateFileA`, `GetProcAddress`, `LoadLibraryA` 등), `USER32.dll`의 `MessageBoxA`·`wsprintfA`, `GDI32.dll`의 `GetStockObject`, `ADVAPI32.dll`의 `RegFlushKey`, `DSOUND.dll` ordinal `#1`, `WINMM.dll`의 `mixerGetControlDetailsA`, `DDRAW.dll`의 `DirectDrawEnumerateA`가 전부입니다.

launcher의 HLE 준비는 모두 이 표에서 IAT 슬롯을 찾으므로 각 경계의 성립 여부가 전부 설명됩니다.

- `GetWindowsDirectoryA` 없음 → `handoff_prepared=false`
- `DirectDrawCreate`·`DirectDrawCreateEx` 없음 → `DirectDraw HLE preparation failed`
- `GetPrivateProfileIntA` 없음 → `demo_volume_prepared=false`
- `GetCommandLineA` 있음 → `--hle-command-line`만으로 `handoff_prepared=true`, `iat_verified=true`
- `DSOUND.dll` ordinal `#1` 있음 → `directsound_prepared=true`

**장치와 IOCTL.** 이 빌드는 `\\.\NTICE`를 열려다 실패(error 123)한 뒤 `\\.\FEnteDev`를 엽니다. `\\.\LPTDI`는 열지 않습니다. 장치 API는 `GetProcAddress`로 해석되어 호출되므로 dynamic resolver가 필요합니다.

**`hle_wts_active_console` 영향 없음.** `--device-mock-wts-console-session`을 넣은 실행과 뺀 실행이 initialize 2회, handshake 2회, descriptor 18회, transform 17회로 완전히 동일했습니다. 근거가 없으므로 켜지 않았습니다.

### 코드 변경

`ez2dj1stse` built-in profile:

| 항목 | 변경 |
| --- | --- |
| `hle_windows_directory` | `true` → `false` |
| `hle_d3d3` | `true` → `false` |
| `demo_volume` | `3` → 해제 |
| `hle_dynamic_vfs` | 미설정 → `true` |
| `device_mock_path_prefix` | `\\.\LPTDI` → `\\.\FEnteDev` |
| `device_mock_target_state_hex` | `0900000000000000` → 해제 |
| `hardlock_cfg_material_default` | 미설정 → `true` |
| `hle_command_line`, `hle_directsound`, `legacy_io_ports`, helper RVA, `run_detached` | 유지 |

profile note를 관측된 경계로 교체했습니다. `tests/unit/target_profile_test.cpp`는 정정된 값을 고정하고, `windows_product_loader_probe`의 1st SE 인자 계약은 21개에서 15개로 줄었습니다.

### 검증

- `cmake --build build/windows-x86 --config Release --target re2dj re2dj_unit_tests re2dj_windows_product_loader_probe` 성공
- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → `profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`
- launcher probe로 정정된 기본값과 동일한 옵션 조합을 실행해 `handoff_prepared=true`, `d3d3_prepared=true`, `directsound_prepared=true`, `demo_volume_prepared=true`, `vfs_prepared=true`, `io_runtime_prepared=true`, `iat_verified=true`를 확인했습니다. 로그는 `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-075917-741`입니다.

### 검증하지 못한 항목

`re2dj ez2dj1stse` 제품 경로의 end-to-end 실행은 확인하지 못했습니다. 앞선 진단 실행이 남긴 게스트 자식 프로세스 두 개(PID 6112, 33148)가 staging 이미지 `%TEMP%\re2dj\chd\ez2dj1stse\ez2dj\Ez2DJ.exe`를 잡고 있고, `Stop-Process`와 `taskkill` 모두 Access denied로 종료되지 않아 staging 파일을 다시 쓸 수 없습니다.

대신 product loader probe가 profile이 만들어 내는 인자 목록을 정확히 고정하고, launcher probe가 그 인자 조합으로 실제 실행되는 것을 확인했습니다. 두 프로세스가 종료된 뒤 `re2dj ez2dj1stse`를 한 번 실행해 확인하는 것이 남은 절차입니다.

### 남은 과제

- 잔여 프로세스 종료 후 `re2dj ez2dj1stse` end-to-end 실행 확인
- `.protect` 빌드의 유효한 Hardlock transform 응답 확인
- 복호화 이후 import directory를 다시 읽어 graphics HLE를 연결하는 경로 조사
- legacy I/O helper RVA가 이 빌드에서도 유효한지 실행으로 확인

## English

### Related documents

- Design: [ez2dj1stse CHD Profile Execution Policy Correction Design](../design/20260908-223-ez2dj1stse-chd-profile-correction.md)
- Work order: [ez2dj1stse CHD Profile Execution Policy Correction](../work-orders/20260908-223-ez2dj1stse-chd-profile-correction.md)
- Analysis: [ez2dj1stse CHD Filesystem Analysis](../analysis/ez2dj1stse-chd-filesystem.md)
- Procedure: [Hardlock descriptor ID extraction](../guides/hardlock-descriptor-extraction.md)

### What was done

The Hardlock descriptor extraction procedure was applied to the CHD `.protect` build to observe its device and IOCTL boundary. The executable's packed import directory was parsed directly to establish the complete set of imports the launcher can patch. Options were then varied one at a time to determine which HLE boundaries can be prepared and whether `hle_wts_active_console` has any effect. The `ez2dj1stse` execution defaults in `target_profile.cpp` were corrected accordingly, the target-profile unit test and the product-loader probe's argument contract were aligned, and the analysis document, `ARCHITECTURE.md`, and `README.md` were updated.

### Confirmed facts

**Packed import directory.** The import data directory sits at RVA `0x01aebbd0` inside `.protect`; the original `.idata` at RVA `0x01aba000` remains in the file but the header no longer points at it. The statically visible imports are fifteen `KERNEL32.dll` entries (including `GetCommandLineA`, `CreateFileA`, `GetProcAddress`, and `LoadLibraryA`), `USER32.dll`'s `MessageBoxA` and `wsprintfA`, `GDI32.dll`'s `GetStockObject`, `ADVAPI32.dll`'s `RegFlushKey`, `DSOUND.dll` ordinal `#1`, `WINMM.dll`'s `mixerGetControlDetailsA`, and `DDRAW.dll`'s `DirectDrawEnumerateA`.

Every launcher HLE preparation step locates its IAT slot in that table, which explains each boundary completely: `GetWindowsDirectoryA` is absent so `handoff_prepared` was false; neither `DirectDrawCreate` nor `DirectDrawCreateEx` is present so DirectDraw HLE preparation failed; `GetPrivateProfileIntA` is absent so `demo_volume_prepared` was false; `GetCommandLineA` is present so `--hle-command-line` alone yielded `handoff_prepared=true` and `iat_verified=true`; and `DSOUND.dll` ordinal `#1` is present so `directsound_prepared` was true.

**Device and IOCTLs.** The build tries `\\.\NTICE`, fails with error 123, then opens `\\.\FEnteDev`. It never opens `\\.\LPTDI`. Its device APIs are called through `GetProcAddress`-resolved pointers, so the dynamic resolver is required.

**`hle_wts_active_console` has no effect.** Runs with and without `--device-mock-wts-console-session` recorded an identical sequence — two initialize, two handshake, eighteen descriptor, and seventeen transform requests — so it was not enabled without evidence.

### Code change

The `ez2dj1stse` built-in profile changes `hle_windows_directory` and `hle_d3d3` from true to false, unsets `demo_volume`, enables `hle_dynamic_vfs`, changes `device_mock_path_prefix` from `\\.\LPTDI` to `\\.\FEnteDev`, clears `device_mock_target_state_hex`, and enables `hardlock_cfg_material_default`. `hle_command_line`, `hle_directsound`, `legacy_io_ports` with its helper RVAs, and `run_detached` are unchanged. The profile note was replaced with the observed boundary, `tests/unit/target_profile_test.cpp` pins the corrected values, and the product-loader probe's 1st SE argument contract shrank from twenty-one arguments to fifteen.

### Verification

- `cmake --build build/windows-x86 --config Release --target re2dj re2dj_unit_tests re2dj_windows_product_loader_probe` succeeded.
- `re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`.
- `re2dj_windows_product_loader_probe.exe` reported `profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`.
- Running the launcher probe with the same option combination the corrected defaults produce confirmed `handoff_prepared`, `d3d3_prepared`, `directsound_prepared`, `demo_volume_prepared`, `vfs_prepared`, `io_runtime_prepared`, and `iat_verified` all true. The log is `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-075917-741`.

### Not verified

The end-to-end `re2dj ez2dj1stse` product path was not confirmed. Two guest child processes left behind by earlier diagnostic runs (PIDs 6112 and 33148) hold the staged image `%TEMP%\re2dj\chd\ez2dj1stse\ez2dj\Ez2DJ.exe`, and neither `Stop-Process` nor `taskkill` can terminate them — both report access denied — so the staging file cannot be rewritten.

In its place, the product-loader probe pins the exact argument list the profile produces, and the launcher probe confirmed a real run with that combination. Running `re2dj ez2dj1stse` once after those processes exit is the remaining step.

### Remaining work

- Confirm the end-to-end `re2dj ez2dj1stse` run once the leftover processes exit.
- Establish a valid Hardlock transform response for the `.protect` build.
- Investigate re-reading the import directory after decryption so the graphics HLE can be connected.
- Confirm by execution whether the legacy-I/O helper RVAs hold for this build.
