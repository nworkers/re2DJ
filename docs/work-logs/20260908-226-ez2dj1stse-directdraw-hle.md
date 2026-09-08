# 작업 로그: ez2dj1stse DirectDraw HLE 활성화

## 한국어

### 관련 문서

- 작업 지시: [ez2dj1stse DirectDraw HLE 활성화](../work-orders/20260908-226-ez2dj1stse-directdraw-hle.md)
- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)
- 선행 작업: [VFS 게스트 루트 접두사와 CHD 열거 현재 디렉터리](20260908-225-vfs-guest-root-and-chd-enumeration.md)

### 확인된 사실

**두 개의 import 표.** CHD `.protect` 빌드의 PE header가 가리키는 import directory는 RVA `0x01aebbd0`이고 `DDRAW.dll`에서는 `DirectDrawEnumerateA` 하나만 가집니다. 원본 `.idata`는 RVA `0x01aba000`에 그대로 남아 있고 다음을 import합니다.

| 모듈 | 항목 수 | DDRAW 관련 |
| --- | --- | --- |
| `KERNEL32.dll` | 97 | |
| `USER32.dll` | 21 | |
| `GDI32.dll` | 14 | |
| `ADVAPI32.dll` | 1 | |
| `DSOUND.dll` | 1 (ordinal `#1`) | |
| `WINMM.dll` | 8 | |
| `DDRAW.dll` | 2 | `DirectDrawEnumerateA`, **`DirectDrawCreate`** |

즉 게임은 legacy `DirectDrawCreate`를 쓰고 그 슬롯은 원본 `.idata` 안에 있습니다. header가 그 표를 가리키지 않았을 뿐이며, 이전에는 조회가 header directory만 봤기 때문에 찾지 못했습니다.

**자산 실패는 결함이 아니었음.** 선행 실행의 실패 731건은 게임의 정상 탐색 경로입니다. CHD의 `System/Title`에는 `flare.bmp`, `amuse.bmp`, `bg.bmp`, `logo.bmp` 등 31개 항목만 있고 `A_credits_*.bmp`는 없습니다. 게스트는 `System\Title\`을 먼저 시도해 실패한 뒤 `System\Common\`에서 성공적으로 엽니다.

### 코드 변경

| 파일 | 변경 |
| --- | --- |
| `iat_verifier.cpp` | `FindIatSlotsByName`이 header directory를 먼저 보고, 못 찾으면 `.idata` section의 표를 추가 조회한다. header directory만 권위 있는 표로 두어 보조 표의 파싱 실패는 오류가 아니라 불일치로 처리한다 |
| `target_profile.cpp` | `ez2dj1stse`의 `hle_d3d3`를 `true`로 |
| `target_profile_test.cpp` | 정정된 값 고정 |
| `windows_product_loader_probe/main.cpp` | 1st SE 인자 계약 15개 → 16개, `--hle-d3d3` 위치 반영 |

`.idata` section의 RVA가 header directory와 같으면 건너뛰므로, 패킹되지 않은 이미지에서는 조회 대상이 늘지 않습니다.

### 검증

- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → 4개 항목 ok

**실제 실행.** `re2dj ez2dj1stse` 로그 `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-092102-771`.

- `graphics_trace`: `has_create=true`, `has_create_ex=false`, `create_ex_patched=false`
- `ChangeDisplaySettingsExA`가 `640x480x16`으로 흡수됨
- 창 생성 확인: `hwnd=000C0DE0`, `valid=1`, `visible=1`
- primary surface 1개(`caps=0x00002218`, back buffer 1개)와 `128x128` RGB565 off-screen surface 42개 생성
- `GetDC`/`ReleaseDC` 각 42회, `Blt` 8회, `Flip` 8회, `RenderState` 28회
- 60초 시점에 `frame=3330`으로 렌더 루프 진행 중

이전 실행은 자산 971건을 읽은 뒤 `ExitProcess(0xc0000005)`로 끝났습니다. 이제 그 종료가 사라지고 렌더 루프가 계속됩니다. **DirectDraw 슬롯이 비어 있던 것이 그 access violation의 원인이었다는 뜻입니다.**

**회귀 검사.** 3rd와 4th를 45초 제한으로 실행했습니다. 두 경우 모두 `has_create=false`, `has_create_ex=true`이고 `create_ex_patched`가 각각 `true`·`false`로 이전 실행과 동일하며, VFS trace 앞 120줄도 handle·tick 값을 제외하고 같습니다. 두 제품 모두 `DirectDrawCreateEx`를 header directory에서 찾으므로 `.idata` 조회에 도달하지 않습니다.

### 남은 과제

- 렌더 결과의 시각적 확인. 이 작업은 경계 도달과 프레임 진행까지만 확인했다
- `hle_windows_directory`와 `demo_volume`은 여전히 불가. `GetWindowsDirectoryA`와 `GetPrivateProfileIntA`는 두 표 어디에도 없다
- legacy I/O helper RVA가 이 빌드에서도 유효한지 실행으로 확인
- `ChdRelativePath`·`GuestDirectoryExists`·`FindFirstFileA`의 `"EZ2DJ"` 고정 이름을 프로파일에서 유도

## English

### Related documents

- Work order: [ez2dj1stse DirectDraw HLE enablement](../work-orders/20260908-226-ez2dj1stse-directdraw-hle.md)
- Analysis: [ez2dj1stse CHD filesystem analysis](../analysis/ez2dj1stse-chd-filesystem.md)
- Preceding task: [VFS guest root prefix and CHD enumeration working directory](20260908-225-vfs-guest-root-and-chd-enumeration.md)

### Confirmed facts

**Two import tables.** The import directory the CHD `.protect` build's PE header points at sits at RVA `0x01aebbd0` and contributes only `DirectDrawEnumerateA` from `DDRAW.dll`. The original `.idata` survives at RVA `0x01aba000` and imports 97 `KERNEL32.dll` entries, 21 from `USER32.dll`, 14 from `GDI32.dll`, one from `ADVAPI32.dll`, `DSOUND.dll` ordinal `#1`, eight from `WINMM.dll`, and two from `DDRAW.dll` — `DirectDrawEnumerateA` and **`DirectDrawCreate`**.

The game therefore uses legacy `DirectDrawCreate` and its slot lives in the original `.idata`. The header simply does not point at that table, and the lookup previously searched only the header directory.

**The asset failures were not a defect.** The 731 failures in the preceding run are the game's own search path. The CHD's `System/Title` holds 31 entries — `flare.bmp`, `amuse.bmp`, `bg.bmp`, `logo.bmp`, and so on — with no `A_credits_*.bmp`. The guest tries `System\Title\` first, fails, and then opens the file successfully from `System\Common\`.

### Code change

`FindIatSlotsByName` now searches the header's import directory first and the `.idata` section's table only when that finds nothing, keeping the header directory as the sole authoritative table so a malformed secondary table is a non-match rather than an error. `hle_d3d3` is enabled for `ez2dj1stse`, the target-profile test pins the new value, and the product-loader probe's 1st SE argument contract grows from fifteen arguments to sixteen with `--hle-d3d3` in place.

An `.idata` section whose RVA equals the header directory's is skipped, so nothing extra is searched on an unpacked image.

### Verification

`re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, and the product-loader probe reported all four items ok.

The real run is logged at `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-092102-771`. `graphics_trace` reports `has_create=true`, `has_create_ex=false`, and `create_ex_patched=false`. `ChangeDisplaySettingsExA` is absorbed at `640x480x16`, a window is created and visible at `hwnd=000C0DE0`, and the guest creates one primary surface (`caps=0x00002218`, one back buffer) plus 42 off-screen `128x128` RGB565 surfaces. The trace records 42 `GetDC`/`ReleaseDC` pairs, eight `Blt`, eight `Flip`, and 28 `RenderState` calls, and the render loop had reached `frame=3330` at the 60-second bound.

The previous run ended at `ExitProcess(0xc0000005)` after reading 971 assets. That exit is gone and the render loop continues, which means **the empty DirectDraw slot was the cause of that access violation**.

**Regression check.** 3rd and 4th were each run under a 45-second bound. Both report `has_create=false` and `has_create_ex=true` with `create_ex_patched` of `true` and `false` respectively, matching their previous runs, and their first 120 VFS trace lines are identical apart from handle and tick values. Both products find `DirectDrawCreateEx` in the header directory, so neither reaches the `.idata` lookup.

### Remaining work

- Visually confirm the rendered output; this task established only boundary reach and frame progress.
- `hle_windows_directory` and `demo_volume` remain impossible: `GetWindowsDirectoryA` and `GetPrivateProfileIntA` are in neither table.
- Confirm by execution whether the legacy-I/O helper RVAs hold for this build.
- Derive the `"EZ2DJ"` literal in `ChdRelativePath`, `GuestDirectoryExists`, and `FindFirstFileA` from the profile.
