# 작업 로그: VFS 게스트 루트 접두사와 CHD 열거 현재 디렉터리

## 한국어

### 관련 문서

- 설계: [VFS 게스트 루트 접두사와 CHD 열거 현재 디렉터리 설계](../design/20260908-225-vfs-guest-root-and-chd-enumeration.md)
- 작업 지시: [VFS 게스트 루트 접두사와 CHD 열거 현재 디렉터리](../work-orders/20260908-225-vfs-guest-root-and-chd-enumeration.md)
- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)
- 선행 작업: [ez2dj1stse Hardlock 후보 판별](20260908-224-ez2dj1stse-hardlock-candidate-judgement.md)

### 확인된 원인

**결함 1.** `StripGuestRoot`가 매핑된 HDD 루트와 문자열 `"D:\\ez2dj"` 두 가지만 루트로 인정했습니다. 1st SE CHD 빌드는 `SetCurrentDirectory("c:\ez2dj")`를 호출하므로 루트로 인정되지 않고, drive-absolute로 파싱된 뒤 relative로 덮어써져 빈 base와 결합되어 `ez2dj` 한 구성요소로 줄었습니다. VFS 루트가 이미 그 디렉터리이므로 존재 검사가 실패했습니다.

**결함 2.** `Re2djVfsFindFirstFileA`의 CHD 분기가 와일드카드를 포함한 이름 전체를 `ResolveGuestRelativePath`에 넘겼습니다. `ParseGuestPath`는 `*`와 `?`를 파일명에 쓸 수 없는 문자로 거부하므로 해석이 실패하고, fallback이 원본 이름을 그대로 쪼개 디렉터리 부분이 비어 CHD 루트를 훑었습니다.

같은 함수의 native 분기에는 이 결함이 없었습니다. `MapVfsSearchPath`가 패턴을 먼저 떼어 내고 디렉터리만 매핑하기 때문입니다. CHD 없이 도는 VFS probe의 열거 시험이 통과해 온 이유가 이것이며, 결함은 CHD 경로에만 남아 있었습니다.

### 코드 변경

| 파일 | 변경 |
| --- | --- |
| `injected_runtime.cpp` | `g_re2dj_vfs_guest_root` export 추가(기본값 `D:\ez2dj`). `StripGuestRoot`가 고정 문자열 대신 이 값을 본다 |
| `injected_runtime.cpp` | `Re2djVfsFindFirstFileA`가 패턴을 먼저 분리하고 디렉터리만 해석한다. 디렉터리가 없으면 `"."` |
| `windows_x86_launcher_probe/main.cpp` | 프로파일의 `guest_drive_letter`·`guest_directory`로 `profile_guest_root`를 만들어 런타임에 쓰고 `vfs_mount` 진단에 기록한다 |
| `child_process_handoff.{h,cpp}` | bootstrap child 주입 경로에도 같은 값을 쓴다 |
| `windows_vfs_runtime_probe/main.cpp` | 게스트 루트 접두사 시험 추가. `--vfs-enumeration-only`가 window·audio 수명주기 probe를 건너뛰게 수정 |

프로파일에 게스트 경로가 없는 대상은 launcher가 `D:\ez2dj`를 쓰므로 동작이 그대로입니다.

`--vfs-enumeration-only`는 [작업 199](20260906-199-vfs-directory-enumeration-regression.md)에서 "DirectSound 종료 검사와 분리해 실행"할 목적으로 추가되었지만, window·audio probe가 그 앞에서 무조건 실행되어 목적을 달성하지 못하고 있었습니다. 이 작업에서 플래그가 문서대로 동작하도록 고쳤습니다.

### 검증

- `cmake --build build/windows-x86 --config Release` 대상 6개 성공
- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → 4개 항목 ok
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → Debug와 Release 모두 exit 0. 설정값 `C:\ez2dj` 인식, 그 상태에서 `D:\ez2dj` 거부, 기본값 복원 후 재인식까지 통과

**실제 실행.** `re2dj ez2dj1stse` 로그 `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-084445-032`.

| 항목 | 수정 전 | 수정 후 |
| --- | --- | --- |
| `set:request=c:\ez2dj` | `resolved=ez2dj:success=0` | `resolved=:success=1` |
| `find-first` | `chd_dir=EZ2DJ:matches=10` | `chd_dir=EZ2DJ/System/Title:matches=31` |
| `set:request=Songs` | `resolved=System/Title/Songs:success=0` | 호출 자체가 사라지고 저장한 native 경로로 복귀 |
| `.vfs.log` 줄 수 | 316 | 2,372 |
| 자산 개방 | 11 | 971 |

`vfs_mount` 진단에 `"guest_root":"C:\\ez2dj"`가 기록됩니다.

게스트는 이제 타이틀(`System\Title\title.str`), 공용 UI(`System\Common\A_credits_*.bmp`, `WAIT_CLUB.bmp`), ClubMix 디스크 그래픽, 곡 폴더(`Songs\reggae-rm\ez\...`)까지 읽습니다.

**회귀 검사.** 3rd와 4th를 45초 제한으로 실행해 비교했습니다. 두 경우 모두 `guest_root`가 `D:\ez2dj`로 기록되고, 이전 실행과 앞 120줄이 handle·tick 값을 제외하고 완전히 동일하며, 실패 항목 집합도 같습니다. 줄 수 차이는 이번 실행을 제한 시간에 종료했기 때문이며, 4th는 오히려 자산 개방이 3건에서 10건으로 늘었습니다.

### 기존 결함 발견 — 이 작업 범위 밖

`re2dj_windows_vfs_runtime_probe`를 옵션 없이 전체 실행하면 이 환경에서 통과하지 못합니다. Release는 `0xC0000409`(stack buffer overrun)로 죽고 Debug는 `RunAudioExitChild`의 DirectSound 생성 실패로 `3`을 반환합니다. 변경 전 HEAD에서도 동일하게 재현되므로 **이 작업이 만든 문제가 아닙니다.** window·audio 수명주기 probe가 데스크톱과 오디오 장치를 요구하는 데서 오는 것으로 **추정**되며, 원인은 확인하지 않았습니다. 경로 검사는 `--vfs-enumeration-only`로 분리해 검증했습니다.

### 남은 과제

- `re2dj ez2dj1stse`가 끝나는 `ExitProcess(0xc0000005)`의 원인 확인. packed import 제약으로 `hle_d3d3`가 꺼져 있어 graphics 경계가 유력 후보
- 복호화 이후 import directory를 다시 읽어 DirectDraw를 HLE로 연결하는 경로 조사
- `ChdRelativePath`·`GuestDirectoryExists`·`FindFirstFileA`의 `"EZ2DJ"` 고정 이름을 프로파일에서 유도
- VFS runtime probe 전체 실행이 이 환경에서 통과하지 못하는 기존 결함 조사

## English

### Related documents

- Design: [VFS Guest Root Prefix and CHD Enumeration Working Directory Design](../design/20260908-225-vfs-guest-root-and-chd-enumeration.md)
- Work order: [VFS Guest Root Prefix and CHD Enumeration Working Directory](../work-orders/20260908-225-vfs-guest-root-and-chd-enumeration.md)
- Analysis: [ez2dj1stse CHD filesystem analysis](../analysis/ez2dj1stse-chd-filesystem.md)
- Preceding task: [ez2dj1stse Hardlock candidate judgement](20260908-224-ez2dj1stse-hardlock-candidate-judgement.md)

### Confirmed causes

**Defect 1.** `StripGuestRoot` recognized only the mapped HDD root and the literal `"D:\\ez2dj"`. The 1st SE CHD build calls `SetCurrentDirectory("c:\ez2dj")`, which was therefore not recognized as a root: it parsed as drive-absolute, had its kind overwritten to relative, combined with an empty base, and reduced to the single component `ez2dj`. Since the VFS root already is that directory, the existence check failed.

**Defect 2.** The CHD branch of `Re2djVfsFindFirstFileA` passed the whole name, wildcard included, to `ResolveGuestRelativePath`. `ParseGuestPath` rejects `*` and `?` as filename characters, so resolution failed and the fallback split the raw name, leaving the directory part empty and sweeping the CHD root.

The same function's native branch did not have this defect, because `MapVfsSearchPath` splits the pattern off first and maps only the directory. That is why the enumeration test in the CHD-less VFS probe kept passing while the defect persisted on the CHD path.

### Code change

The injected runtime gains a `g_re2dj_vfs_guest_root` export defaulting to `D:\ez2dj`, and `StripGuestRoot` consults it instead of a literal. `Re2djVfsFindFirstFileA` now splits the pattern first and resolves only the directory, using `"."` when there is none. The launcher builds `profile_guest_root` from the profile's `guest_drive_letter` and `guest_directory`, writes it to the runtime, records it in the `vfs_mount` diagnostic, and passes the same value on the bootstrap-child injection path. The VFS runtime probe gains a guest-root-prefix test, and `--vfs-enumeration-only` now skips the window and audio lifecycle probes.

Targets whose profiles record no guest path receive `D:\ez2dj` and are unchanged.

`--vfs-enumeration-only` was added in [task 199](20260906-199-vfs-directory-enumeration-regression.md) to run the path checks apart from the DirectSound shutdown check, but the window and audio probes ran unconditionally ahead of it and defeated that purpose. This task makes the flag behave as documented.

### Verification

Six Release targets built. `re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, and the product-loader probe reported all four items ok. `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0 in both Debug and Release, covering recognition of a configured `C:\ez2dj`, rejection of `D:\ez2dj` while that is configured, and recognition again after the default is restored.

The decisive check is the real run, logged at `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-084445-032`.

| Item | Before | After |
| --- | --- | --- |
| `set:request=c:\ez2dj` | `resolved=ez2dj:success=0` | `resolved=:success=1` |
| `find-first` | `chd_dir=EZ2DJ:matches=10` | `chd_dir=EZ2DJ/System/Title:matches=31` |
| `set:request=Songs` | `resolved=System/Title/Songs:success=0` | the call disappears; the guest returns to its saved native path |
| `.vfs.log` lines | 316 | 2,372 |
| asset opens | 11 | 971 |

The `vfs_mount` diagnostic records `"guest_root":"C:\\ez2dj"`. The guest now reads the title screen (`System\Title\title.str`), the shared UI (`System\Common\A_credits_*.bmp`, `WAIT_CLUB.bmp`), ClubMix disc graphics, and song folders (`Songs\reggae-rm\ez\...`).

**Regression check.** 3rd and 4th were each run under a 45-second bound. Both record `guest_root` as `D:\ez2dj`, their first 120 trace lines are identical to the previous runs apart from handle and tick values, and their failure sets match. The line-count difference comes from ending these runs at the bound; 4th in fact opened ten assets against the previous three.

### Pre-existing defect found — out of scope here

Running `re2dj_windows_vfs_runtime_probe` with no options does not pass in this environment: Release dies with `0xC0000409` (stack buffer overrun) and Debug returns `3` from `RunAudioExitChild` when DirectSound creation fails. Both reproduce at HEAD before this change, so **this task did not cause them**. The window and audio lifecycle probes requiring a desktop and an audio device are the **inferred** reason; the cause was not confirmed. The path checks were verified separately through `--vfs-enumeration-only`.

### Remaining work

- Confirm the cause of the `ExitProcess(0xc0000005)` that ends the `re2dj ez2dj1stse` run; the graphics boundary is the leading candidate because `hle_d3d3` is off under the packed-import constraint.
- Investigate re-reading the import directory after decryption so DirectDraw can be connected to the HLE.
- Derive the `"EZ2DJ"` literal in `ChdRelativePath`, `GuestDirectoryExists`, and `FindFirstFileA` from the profile.
- Investigate the pre-existing failure of the full VFS runtime probe in this environment.
