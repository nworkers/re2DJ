# ez2d2m CHD 파일시스템과 실행 파일 관찰

## 한국어

### 범위와 상태

이 문서는 사용자가 제공한 `roms/ez2d2m/ez2d2m.chd`를 현재 CHD/FAT32 reader와 `re2dj_pe_analyzer`, 그리고 `dumpbin /imports`로 읽은 결과를 기록합니다. 대상은 EZ2DJ가 아니라 **EZ2Dancer 2nd MOVE**입니다. 원본 이미지, 실행 파일, 자산은 저장소에 포함하지 않습니다.

이 문서의 모든 서술은 **정적 관찰**입니다. 이 실행 파일을 re2DJ로 실행한 적은 없으므로, 런타임 동작은 어느 것도 확인되지 않았습니다.

### 확인됨 — 이미지와 볼륨

- CHD v5, logical bytes `134,754,762,240`, hunk bytes `4096`, unit bytes `512`, hunk count `32,899,112`.
- codec set은 `lzma,zlib,huff,flac`이고 metadata는 `GDDD` 하나로 `CYLS:16383,HEADS:255,SECS:63,BPS:512`입니다.
- LBA 0은 `55aa` 서명을 가진 MBR이며, partition 0이 FAT32입니다. partition LBA `63`, partition sectors `29,334,627`.
- BPB: bytes per sector `512`, sectors per cluster `16`, reserved sectors `36`, FAT 2개, sectors per FAT `14,310`, root cluster `2`, data LBA `28,719`, cluster count `1,831,623`.
- volume label은 `EZ2DANCER`입니다.
- 선언 용량은 약 15 GiB지만 관찰된 최대 first cluster는 `342,044`이므로 실제 사용량은 약 2.8 GiB입니다.

### 확인됨 — 디스크는 Windows 98 SE 부팅 볼륨입니다

- 루트에 `IO.SYS`, `MSDOS.SYS`, `COMMAND.COM`, `AUTOEXEC.BAT`, `CONFIG.SYS`, `WINDOWS`, `Program Files`가 있습니다.
- `MSDOS.SYS`는 `WinDir=C:\WINDOWS`, `WinBootDir=C:\WINDOWS`, `HostWinBootDrv=C`, `WinVer=4.10.2222`입니다. `4.10.2222`는 Windows 98 SE입니다.
- `WINDOWS/SYSTEM.INI`의 `[boot]`는 `shell=Explorer.exe`입니다. 따라서 게임은 shell이 아니라 Explorer 아래에서 시작합니다. 1st SE CHD와 같은 배치입니다.
- `AUTOEXEC.BAT`는 `******************** Amuseworld System IV ********************`를 출력합니다.
- 루트에 `Install`과 `Hardware Tools` 디렉터리가 있습니다. `Install`에는 `Win98SE`, `DirectX 7.0a Korean`, `Detonator 5.32 Win9x`, `4in1423`, `Ultra66`, `LiveWare 3.0`, BIOS 이미지가 있습니다. `Hardware Tools`에는 `DOS4GW.EXE`, `outport.exe`, `testport.exe`, `FOOTEST.EXE`, `LASERON.EXE`와 `TEST2`~`TEST10` 계열 DOS 실행 파일이 있습니다.

`DirectX 7.0a`가 설치본에 있다는 사실은 아래의 `DirectDrawCreateEx` 관찰과 일치합니다.

### 확인됨 — 게임 디렉터리

`ez2dancer` 디렉터리에 다음 8개 항목이 있습니다.

| 이름 | 종류 | 크기 |
| --- | --- | --- |
| `EZ2Dancer.exe` | 파일 | 622,592 |
| `EZ2DANCER.ini` | 파일 | 712 |
| `song.ini` | 파일 | 21,008 |
| `upgrade.ini` | 파일 | 200 |
| `fonten.dat` | 파일 | 2,560 |
| `fontkr.dat` | 파일 | 75,200 |
| `Songs` | 디렉터리 | 61 항목 |
| `SYSTEM` | 디렉터리 | 32 항목 |

`SYSTEM` 아래에는 `Title`, `Opening`, `Opening-1st`, `Opening-2nd`, `select_music`, `MODE`, `RESULT`, `totalresult`, `Ranking`, `InternetRanking`, `GAMEOVER`, `EYECATCH`, `course-eyecatch`, `course-md`, `DISC-MD`, `channel_preview`, `HOWTO`, `HOWREAL`, `HOWENG`, `howengreal`, `ost`, `soundFX`, `TestMode`, `COMMON`과 여섯 개의 `Panel*` 변종(`Panel`, `panel-panic`, `panel-star`, `panel-TOMATO`, `Panel-TURTLE`, `panel-ztar`)이 있고, 파일로 `WARNING.abm`(921,656바이트)과 `sensoreffect.ezw`(17,618바이트)가 있습니다.

**확인됨:** `EZ2DANCER.ini`, `upgrade.ini`, `song.ini`는 평문이 아닙니다. 세 파일 모두 인쇄 가능한 ASCII가 아닌 바이트로 시작합니다. **미확정:** 그 형식과 암호화 방식.

### 확인됨 — `ez2dancer/EZ2Dancer.exe` PE 구조

- PE32 / i386, image base `0x00400000`, entry point RVA `0x00401240`, SizeOfImage `0x0043b000`, subsystem 2 (windows-gui) 4.0.
- timestamp `0x3a5f074c`, section alignment와 file alignment 모두 `0x1000`.
- 섹션 4개:

| 이름 | vaddr | vsize | raw off | raw size | flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x0004b93e` | `0x00001000` | `0x0004c000` | `0x60000020` |
| `.rdata` | `0x0004d000` | `0x00004c10` | `0x0004d000` | `0x00005000` | `0xc0000040` |
| `.data` | `0x00052000` | `0x003ae42c` | `0x00052000` | `0x0000c000` | `0xc0000040` |
| `.protect` | `0x00401000` | `0x000399cd` | `0x0005e000` | `0x0003a000` | `0xe0000020` |

- import directory는 RVA `0x0043a810`, 크기 `0x1bd`으로 `.protect` 안에 있습니다. base relocation은 RVA `0x00402000`, 크기 `0x60`입니다.

**확인됨:** entry point가 `.protect` 안에 있고 `.protect`가 쓰기와 실행 권한을 모두 가지므로, 이 실행 파일은 EZ2DJ 1st·1st SE·3rd·4th·5th와 같은 자기 수정 packer 계열입니다. 실행하려면 자기 수정 코드를 견디는 backend가 필요합니다.

### 확인됨 — packed import directory

`dumpbin /imports`가 헤더의 import directory에서 읽는 descriptor는 7개이고, 세 무리로 나뉩니다.

1. RVA `0x00428db0` 부근 — 보호 계층 자신의 KERNEL32 집합: `CreateFileA`, `CloseHandle`, `ReadFile`, `WriteFile`, `GetFileSize`, `FindFirstFileA`, `FindNextFileA`, `FindClose`, `GetProcAddress`, `LoadLibraryA`, `FreeLibrary`, `GetModuleHandleA`, `GetModuleFileNameA`, `GetEnvironmentVariableA`, `GetCurrentProcessId`, `GetVersion`, `GetLocalTime`, `GetSystemTime`, `SystemTimeToFileTime`, `LocalAlloc`, `LocalFree`, `SetErrorMode`, `Sleep`, `lstrcmpA`, `lstrlenA`.
2. RVA `0x00401f90` 부근 — unpacker stub: KERNEL32의 `GetModuleHandleA`와 `GetProcAddress`, USER32의 `MessageBoxA`.
3. RVA `0x0043a8e0` 부근 — DLL당 stub 하나: KERNEL32 `WaitCommEvent`, USER32 `DrawTextA`, GDI32 `SetTextColor`, **DDRAW `DirectDrawCreateEx`**, DSOUND ordinal `1`, WINMM `mixerGetLineInfoA`.

이 표에서 직접 따라오는 결론입니다.

- **확인됨:** `GetCommandLineA`, `GetWindowsDirectoryA`, `GetPrivateProfileIntA`가 모두 없습니다. 세 HLE 경계는 이 빌드에서 준비할 수 없습니다.
- **확인됨:** DDRAW 진입점이 `DirectDrawCreate`가 아니라 `DirectDrawCreateEx`입니다. DirectDraw 7 경로입니다.
- **확인됨:** DSOUND ordinal 1(`DirectSoundCreate`)이 있습니다.
- **확인됨:** 원본 `.rdata`의 문자열은 `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `DDRAW.dll`, `DSOUND.dll`, `DINPUT.dll`, `AVIFIL32.dll`, `WINMM.dll`, `WS2_32.dll`을 나열합니다. packed table에는 `DINPUT`, `AVIFIL32`, `WS2_32`가 없으므로, 원본이 그 셋을 쓴다면 packer가 unpack 시점에 직접 해결합니다.

### 확인됨 — 보호 계층은 EZ2DJ와 같은 Hardlock envelope입니다

실행 파일 문자열에 다음이 모두 있습니다.

- 장치: `\\.\HARDLOCK.VXD`, `\\.\FEnteDev` (`FEnteDev`는 네 번 등장)
- Hardlock 런타임: `HLW32Proc`, `API_1LNM.DLL`, `HL_SEARCH`, `HL_LICENSEDIR`, `hlrus_license_file.alf`, `Begin HL-RUS License ----------------------`
- 세션 조회: `ProcessIdToSessionId`, `wtsapi32.dll`, `WTSQuerySessionInformationA`, `WTSFreeMemory`, `wfapi.dll`, `WFQuerySessionInformationA`, `Terminal Server`, `System\CurrentControlSet\Control\ProductOptions`
- 동적 해결: `kernel32.dll`, `DeviceIoControl`, `advapi32.dll`, `RegOpenKeyA`, `RegQueryValueExA`, `IsTNT`, `Borland32`

**확인됨:** 장치 이름, Hardlock API DLL, 세션 조회 경로가 EZ2DJ 3rd·4th·5th와 같습니다. **미확정:** 이 빌드의 Hardlock 응답, descriptor의 `module_address`, `id_ref`, `id_verify`. 이들은 실행해야 얻을 수 있고 이 작업에서는 시도하지 않았습니다.

`DeviceIoControl`이 정적 import가 아니라 문자열로 등장한다는 점은 다른 `.protect` 빌드와 같으며, 장치 작업이 `GetProcAddress`를 거친다는 뜻입니다. 그래서 `ez2d2m` 프로파일은 `hle_dynamic_vfs`를 켭니다.

### 확인됨 — 게임 자신의 문자열

- 창/객체 이름: `KEz2DancerObject`, `EZ2Dancer`
- 설정: `Ez2Dancer.ini`, `song.ini`, `system\%s\panel.ini`, `%s\%s.ini`, `Version`, `TotalNonPlayTime`
- 자산: `songs\%s\%s.scr`, `%s%s.bmp`, `system\TestMode\ColorBar.bmp`, `system\TestMode\Grid.bmp`
- 데모: `demo1`, `demo2`, `~&rdemo3`
- 업그레이드: `D:\Upgrade.exe`, `D:\UPGRADE.EXE`, `Please insert Upgrade CD into CD-ROM drive`
- 그래픽 오류: `KGraphics: Can't create DDraw during enumeration!`, `KGraphics: Error: DDraw object is still referenced!`
- 테스트 모드: `VideoTest`, `Service Button to Select,  Test Button to Exit`

**확인됨:** 자산 경로가 `system\`과 `songs\`로 시작하는 게스트 상대 경로이고, 실행 파일과 같은 디렉터리를 기준으로 합니다. 이는 `ez2dancer` 디렉터리의 실제 배치와 일치합니다.

### 미확정

- 이 빌드의 Hardlock transform 응답. descriptor 자체는 아래 2026-09-10 절에서 확인됐습니다
- raw I/O helper의 실제 RVA. EZ2DJ 계열의 값은 다른 빌드의 것이므로 옮겨 쓰지 않았습니다. [EZ2Dancer I/O 포트 맵](ez2dancer-io-map.md) 참조
- `EZ2DANCER.ini`, `song.ini`, `upgrade.ini`의 형식
- `.abm`, `.ezw`, `.scr` 자산 형식
- `DirectDrawCreateEx` 경로에서 이 게임이 요구하는 표면 형식과 디바이스
- re2DJ에서의 실제 실행 성공 여부. 아직 시도하지 않았습니다

## English

### Scope and status

This document records what the current CHD/FAT32 reader, `re2dj_pe_analyzer` and `dumpbin /imports` report for the user-supplied `roms/ez2d2m/ez2d2m.chd`. The target is **EZ2Dancer 2nd MOVE**, not an EZ2DJ release. The original image, executable and assets are not added to the repository.

Everything here is a **static observation**. This executable has never been run under re2DJ, so no runtime behaviour is confirmed.

### Confirmed — image and volume

The image is CHD v5 with `134,754,762,240` logical bytes, 4096-byte hunks, 512-byte units and `32,899,112` hunks. Its codec set is `lzma,zlib,huff,flac` and its single `GDDD` metadata reads `CYLS:16383,HEADS:255,SECS:63,BPS:512`. LBA 0 is an MBR with a `55aa` signature whose partition 0 is FAT32 at LBA `63` across `29,334,627` sectors. The BPB gives 512 bytes per sector, 16 sectors per cluster, 36 reserved sectors, two FATs of `14,310` sectors, root cluster `2`, data LBA `28,719` and `1,831,623` clusters. The volume label is `EZ2DANCER`. The declared capacity is roughly 15 GiB, but the highest observed first cluster is `342,044`, so about 2.8 GiB is in use.

### Confirmed — the disk is a Windows 98 SE boot volume

The root holds `IO.SYS`, `MSDOS.SYS`, `COMMAND.COM`, `AUTOEXEC.BAT`, `CONFIG.SYS`, `WINDOWS` and `Program Files`. `MSDOS.SYS` reads `WinDir=C:\WINDOWS`, `HostWinBootDrv=C` and `WinVer=4.10.2222`, which is Windows 98 SE. `WINDOWS/SYSTEM.INI` has `shell=Explorer.exe` under `[boot]`, so the game starts under Explorer rather than as the shell — the same arrangement as the 1st SE CHD. `AUTOEXEC.BAT` prints `******************** Amuseworld System IV ********************`.

The root also holds `Install` and `Hardware Tools`. `Install` contains `Win98SE`, `DirectX 7.0a Korean`, `Detonator 5.32 Win9x`, `4in1423`, `Ultra66`, `LiveWare 3.0` and BIOS images; `Hardware Tools` contains `DOS4GW.EXE`, `outport.exe`, `testport.exe`, `FOOTEST.EXE`, `LASERON.EXE` and a `TEST2` through `TEST10` family of DOS executables. The presence of DirectX 7.0a agrees with the `DirectDrawCreateEx` observation below.

### Confirmed — the game directory

`ez2dancer` holds eight entries: `EZ2Dancer.exe` (622,592 bytes), `EZ2DANCER.ini` (712), `song.ini` (21,008), `upgrade.ini` (200), `fonten.dat` (2,560), `fontkr.dat` (75,200), the `Songs` directory (61 entries) and the `SYSTEM` directory (32 entries).

`SYSTEM` contains `Title`, `Opening`, `Opening-1st`, `Opening-2nd`, `select_music`, `MODE`, `RESULT`, `totalresult`, `Ranking`, `InternetRanking`, `GAMEOVER`, `EYECATCH`, `course-eyecatch`, `course-md`, `DISC-MD`, `channel_preview`, `HOWTO`, `HOWREAL`, `HOWENG`, `howengreal`, `ost`, `soundFX`, `TestMode`, `COMMON` and six `Panel*` variants (`Panel`, `panel-panic`, `panel-star`, `panel-TOMATO`, `Panel-TURTLE`, `panel-ztar`), plus the files `WARNING.abm` (921,656 bytes) and `sensoreffect.ezw` (17,618 bytes).

**Confirmed:** `EZ2DANCER.ini`, `upgrade.ini` and `song.ini` are not plaintext; all three begin with non-printable bytes. **Unresolved:** their format and encryption.

### Confirmed — `ez2dancer/EZ2Dancer.exe` PE structure

It is a PE32/i386 windows-gui 4.0 image with base `0x00400000`, entry point RVA `0x00401240`, SizeOfImage `0x0043b000`, timestamp `0x3a5f074c`, and `0x1000` section and file alignment. Its four sections are `.text` (vaddr `0x00001000`, vsize `0x0004b93e`), `.rdata` (`0x0004d000`, `0x00004c10`), `.data` (`0x00052000`, `0x003ae42c`) and `.protect` (`0x00401000`, `0x000399cd`, flags `0xe0000020`). The import directory sits at RVA `0x0043a810` with size `0x1bd`, inside `.protect`; base relocations are at RVA `0x00402000`, size `0x60`.

**Confirmed:** the entry point lies inside a writable, executable `.protect` section, so this executable is the same self-modifying packer family as EZ2DJ 1st, 1st SE, 3rd, 4th and 5th, and running it needs a backend that tolerates self-modifying code.

### Confirmed — the packed import directory

`dumpbin /imports` reads seven descriptors from the header's import directory, in three groups.

Around RVA `0x00428db0` is the protection's own KERNEL32 set: `CreateFileA`, `CloseHandle`, `ReadFile`, `WriteFile`, `GetFileSize`, `FindFirstFileA`, `FindNextFileA`, `FindClose`, `GetProcAddress`, `LoadLibraryA`, `FreeLibrary`, `GetModuleHandleA`, `GetModuleFileNameA`, `GetEnvironmentVariableA`, `GetCurrentProcessId`, `GetVersion`, `GetLocalTime`, `GetSystemTime`, `SystemTimeToFileTime`, `LocalAlloc`, `LocalFree`, `SetErrorMode`, `Sleep`, `lstrcmpA` and `lstrlenA`.

Around RVA `0x00401f90` is the unpacker stub: KERNEL32's `GetModuleHandleA` and `GetProcAddress` plus USER32's `MessageBoxA`.

Around RVA `0x0043a8e0` is one stub per DLL: KERNEL32 `WaitCommEvent`, USER32 `DrawTextA`, GDI32 `SetTextColor`, **DDRAW `DirectDrawCreateEx`**, DSOUND ordinal `1` and WINMM `mixerGetLineInfoA`.

Three conclusions follow directly. **Confirmed:** `GetCommandLineA`, `GetWindowsDirectoryA` and `GetPrivateProfileIntA` are all absent, so those three HLE boundaries cannot be prepared for this build. **Confirmed:** the DDRAW entry point is `DirectDrawCreateEx`, not `DirectDrawCreate` — the DirectDraw 7 path. **Confirmed:** DSOUND ordinal 1 is present. Separately, the original `.rdata` strings list `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `DDRAW.dll`, `DSOUND.dll`, `DINPUT.dll`, `AVIFIL32.dll`, `WINMM.dll` and `WS2_32.dll`; since the packed table has no `DINPUT`, `AVIFIL32` or `WS2_32` descriptor, the packer resolves those itself at unpack time if the original uses them.

### Confirmed — the protection is the same Hardlock envelope as EZ2DJ

The executable's strings carry the devices `\\.\HARDLOCK.VXD` and `\\.\FEnteDev` (the latter four times); the Hardlock runtime names `HLW32Proc`, `API_1LNM.DLL`, `HL_SEARCH`, `HL_LICENSEDIR`, `hlrus_license_file.alf` and `Begin HL-RUS License ----------------------`; the session-query path `ProcessIdToSessionId`, `wtsapi32.dll`, `WTSQuerySessionInformationA`, `WTSFreeMemory`, `wfapi.dll`, `WFQuerySessionInformationA`, `Terminal Server` and `System\CurrentControlSet\Control\ProductOptions`; and the dynamic-resolution names `kernel32.dll`, `DeviceIoControl`, `advapi32.dll`, `RegOpenKeyA`, `RegQueryValueExA`, `IsTNT` and `Borland32`.

**Confirmed:** the device names, the Hardlock API DLL and the session-query path match EZ2DJ 3rd, 4th and 5th. **Unresolved:** this build's Hardlock response and its descriptor's `module_address`, `id_ref` and `id_verify` — those require running it, which this task did not attempt.

`DeviceIoControl` appearing as a string rather than a static import matches the other `.protect` builds and means device work goes through `GetProcAddress`, which is why the `ez2d2m` profile enables `hle_dynamic_vfs`.

### Confirmed — the game's own strings

The window and object names are `KEz2DancerObject` and `EZ2Dancer`. Configuration strings are `Ez2Dancer.ini`, `song.ini`, `system\%s\panel.ini`, `%s\%s.ini`, `Version` and `TotalNonPlayTime`. Asset strings are `songs\%s\%s.scr`, `%s%s.bmp`, `system\TestMode\ColorBar.bmp` and `system\TestMode\Grid.bmp`. Demo names are `demo1`, `demo2` and `~&rdemo3`. The upgrade path is `D:\Upgrade.exe` with `Please insert Upgrade CD into CD-ROM drive`. Graphics errors read `KGraphics: Can't create DDraw during enumeration!` and `KGraphics: Error: DDraw object is still referenced!`. Test mode carries `VideoTest` and `Service Button to Select,  Test Button to Exit`.

**Confirmed:** asset paths are guest-relative and begin with `system\` and `songs\`, resolved against the executable's own directory, matching the actual `ez2dancer` layout.

### Unresolved

- This build's Hardlock transform responses. The descriptor itself is confirmed in the 2026-09-10 section below.
- The real raw-I/O helper RVAs. The EZ2DJ values belong to different builds and were not carried over; see [the EZ2Dancer I/O port map](ez2dancer-io-map.md).
- The format of `EZ2DANCER.ini`, `song.ini` and `upgrade.ini`.
- The `.abm`, `.ezw` and `.scr` asset formats.
- The surface formats and device this game requires on the `DirectDrawCreateEx` path.
- Whether it runs at all under re2DJ. This has not been attempted.

---

## 2026-09-10 Hardlock descriptor 관측 / Hardlock descriptor observation

### 한국어

이 절만 정적 관찰이 아니라 **실제 실행 관측**입니다. 절차는 [Hardlock descriptor ID 추출](../guides/hardlock-descriptor-extraction.md)이고, 증거는 `logs/windows_x86_launcher_probe/ez2d2m/20260910-133835-124.jsonl`과 같은 이름의 `.vfs.log`입니다.

#### descriptor에 도달한 조건 — 확인됨

문서화된 절차만으로는 도달하지 못합니다. 1st·5th와 같은 세 가지가 더 필요했습니다.

| 항목 | 없을 때 |
| --- | --- |
| `--hle-dynamic-vfs` | `.protect` packer가 정적 IAT 패치를 덮어써 `CreateFileA`가 `route=win32`가 되고, 게스트가 호스트 실제 장치를 열려다 Hardlock 요청 0건으로 끝납니다 |
| `--run-detached` | launcher가 첫 VFS 파일 개방을 handoff로 보고 원본을 종료시켜, `\.\NTICE`까지도 가지 못합니다 |
| handshake 응답(`--hardlock-device`와 `response450`·`tail44c`) | initialize 1회 뒤 descriptor를 요청하지 않고 끝납니다 |

handshake 재생값은 3rd·4th·1st SE·1st·5th에서 쓰던 것과 같은 값이 그대로 성립합니다. `ez2d2m`을 포함해 **여섯 제품이 같은 handshake 재생값을 씁니다.**

#### 요청 구성 — 확인됨

| IOCTL | 뜻 | 횟수 |
| --- | --- | --- |
| `0x9c402468` | initialize | 1 |
| `0x9c402450` | handshake | 2 |
| `0x9c40244c` | descriptor | 12 |
| `0x9c402458` | transform | 11 |

#### descriptor 내용 — 확인됨

descriptor 12건 모두 `header_valid=1`이고 256바이트이며, 다음 값이 요청 전체에서 하나로 일치합니다.

| 항목 | 값 |
| --- | --- |
| `module_address` | `0x4c5e` |
| `module_id` | `0x0000` |
| `remote` | `0x0001` |
| `port` | `0x0378` |
| `speed`·`network_users`·`block_count` | `0x0000` |
| `function` | `0x0000` 1건, `0x0006` 11건 |
| `id_ref` | non-zero, 요청 전체에서 동일 |
| `id_verify` | non-zero, 요청 전체에서 동일, **`id_ref`와는 다른 값** |

`module_address=0x4c5e`는 5th(`0x4c5c`), 6th(`0x4c51`)와 같은 대역입니다. 1st(`0x15e5`), 1st SE(`0x15e1`) 대역과는 구분됩니다. 이는 관측된 사실이며, 이 대역이 무엇을 뜻하는지는 **미확정**입니다.

`id_ref`와 `id_verify`가 서로 다른 점은 6th와 대비됩니다. 6th는 두 값이 같았습니다.

**원문 ID는 저장소에 기록하지 않습니다.** `cfg/hardlock-id.ini`의 `[ez2d2m]` section에만 있으며, `cfg/`는 전체가 Git에서 무시됩니다.

#### 아직 아닌 것 — 미확정

descriptor에 도달한 것이 보호 통과를 뜻하지는 않습니다. transform 요청 11건에 대한 유효한 응답 map이 없어 게스트는 자기 자산을 하나도 열지 못했고(`asset-open` 0건), 마지막에 `0xc0000005`로 끝납니다. 유효한 `cfg/hardlock-ez2d2m.map` 확보는 별도 작업입니다.

### English

This section alone is **runtime observation** rather than static analysis. The procedure is [Hardlock descriptor ID extraction](../guides/hardlock-descriptor-extraction.md), and the evidence is `logs/windows_x86_launcher_probe/ez2d2m/20260910-133835-124.jsonl` with its matching `.vfs.log`.

#### What it took to reach the descriptor — confirmed

The documented procedure alone does not get there. Three additions were needed, the same ones 1st and 5th needed. Without `--hle-dynamic-vfs` the `.protect` packer overwrites the static IAT patch, `CreateFileA` resolves `route=win32`, and the guest tries the host's real device and ends with zero Hardlock requests. Without `--run-detached` the launcher treats the first VFS file open as the handoff and terminates the original before it even reaches `\.\NTICE`. Without a handshake response — `--hardlock-device` plus `response450` and `tail44c` — it stops after one initialize and never asks for a descriptor.

The handshake replay values are the same ones already used for 3rd, 4th, 1st SE, 1st and 5th: **six products now share one handshake replay.**

#### Request composition — confirmed

One `0x9c402468` initialize, two `0x9c402450` handshakes, twelve `0x9c40244c` descriptors, and eleven `0x9c402458` transforms.

#### Descriptor contents — confirmed

All twelve descriptors are 256 bytes with `header_valid=1`, and these values are identical across every request: `module_address` `0x4c5e`, `module_id` `0x0000`, `remote` `0x0001`, `port` `0x0378`, and `speed`, `network_users` and `block_count` all `0x0000`. The `function` field is `0x0000` once and `0x0006` eleven times. `id_ref` and `id_verify` are both non-zero and constant across requests, and **they differ from each other** — unlike 6th, where the two were identical.

`module_address=0x4c5e` sits in the same band as 5th (`0x4c5c`) and 6th (`0x4c51`), distinct from the 1st (`0x15e5`) and 1st SE (`0x15e1`) band. That is an observation; what the band means is **unresolved**.

**The raw IDs are not recorded in the repository.** They exist only in the `[ez2d2m]` section of `cfg/hardlock-id.ini`, and all of `cfg/` is ignored by Git.

#### What this is not — unresolved

Reaching the descriptor is not passing the protection. With no valid response map for the eleven transform requests the guest opened none of its own assets (zero `asset-open` lines) and ended at `0xc0000005`. Obtaining a valid `cfg/hardlock-ez2d2m.map` is separate work.

---

## 2026-09-10 후보 map 판별 / Candidate map judgement

### 한국어

사용자가 `cfg/ez2d2m/`에 외부 도구 `resoftlock`이 만든 후보 산출물을 두었습니다. 후보 map 95개와 그 생성 근거입니다. 이 절은 그 산출물을 검증하고 원본 실행으로 판별한 결과입니다. `cfg/`는 Git이 무시하므로 산출물 자체는 저장소에 들어오지 않습니다.

#### 산출물 정합성 — 확인됨

| 검사 | 결과 |
| --- | --- |
| 원본 PE SHA-256 | CHD에서 추출한 `ez2dancer/EZ2Dancer.exe`와 일치 |
| `module_address` | seedcfg·mapcfg·런타임 descriptor 모두 `0x4c5e` |
| `id_ref`·`id_verify` | 런타임에서 추출한 값과 일치 |
| shard 병합 | 후보 수 18+23+25+29 = 95, seed3 탐색 10,920×4 = 43,680으로 전 범위 |
| 후보 목록 | 95행, 고유 seed triple 95개, `seed1`·`seed2`·`seed3` 오름차순 |
| map 세트 | 95개, 각 13행, 형식 위반 0, response 조합 95개 모두 상이 |
| map 형식 | 기존 `hardlock-ez2dj3rd.map`과 동일해 `--hardlock-transform-map`이 `entries=13`으로 로드 |

#### challenge 목록의 결함 — 확인됨

정적으로 유도한 challenge 목록이 런타임 요청과 어긋납니다.

- 런타임은 고유 challenge **11개**를 요청하고, 정적 목록은 **13개**입니다.
- 앞 **10개는 순서까지 정확히 일치**합니다.
- 런타임 11번째 challenge가 정적 목록에 **없습니다.** 따라서 후보 95개 중 어느 것도 이 요청에 답할 수 없고, 모든 실행에서 `mapped=10:unmapped=1`이 됩니다.
- 정적 목록의 나머지 3개는 런타임에서 한 번도 요청되지 않습니다. 그 중 하나는 값 자체가 main image 주소 형태여서 오탐으로 보입니다.

seed 후보는 descriptor에서 유도되며 challenge 목록과 독립이므로, 같은 95개 seed에 런타임 목록을 넣어 map 생성 단계만 다시 수행하면 됩니다. 런타임 목록은 `cfg/ez2d2m/runtime-challenges.txt`에 있습니다.

#### 판별 결과 — 확인됨

빠진 challenge가 마지막 요청이므로 앞 10개만으로도 후보가 갈립니다. 후보 95개를 전수 실행한 결과는 정확히 두 부류입니다.

| 부류 | 후보 수 | handshake | descriptor | transform | trace 줄 수 |
| --- | --- | --- | --- | --- | --- |
| 1차 라운드에서 종료 | 94 | 2 | 12 | 11 | 227 또는 229 |
| **`candidate-70`** | **1** | **4** | **13** | 11 | **240** |

`candidate-70`만 1차 라운드를 넘어 `\.\FEnteDev`를 다시 열고 handshake 2회와 descriptor를 한 번 더 수행합니다. 즉 **2차 보호 라운드에 진입하는 유일한 후보**입니다. 재실행에서도 같은 수치가 나옵니다. 산출물 README가 기존 다섯 제품의 쌍 B를 근거로 예측한 후보와 일치합니다.

전수 결과는 `cfg/ez2d2m/judgement-sweep.txt`에 있습니다.

#### 이것이 뜻하지 않는 것 — 미확정

`candidate-70`도 `0xc0000005`로 한 번 죽습니다. 240줄 중 126번째 줄, 마지막 transform 직후입니다. 다른 점은 그 뒤로 실행이 이어져 2차 라운드까지 간다는 것뿐입니다.

- 어느 후보도 자산을 열지 못했습니다(`asset-open` 0건).
- `candidate-70`이 **정답 map이라고 확정할 수 없습니다.** 확정된 것은 95개 중 유일하게 더 진행한다는 사실입니다.
- 126번째 줄의 crash가 빠진 11번째 challenge에 잘못된 답을 준 결과인지는 확인되지 않았습니다. 위치상 그럴듯하지만 근거가 없습니다.
- challenge 목록을 고쳐 map을 다시 만든 뒤 재판별하는 것이 다음 단계입니다.

### English

The repository owner placed candidate artifacts produced by the external `resoftlock` tool under `cfg/ez2d2m/`: 95 candidate maps and the evidence behind them. This section records verifying those artifacts and judging them against the original executable. `cfg/` is Git-ignored, so the artifacts themselves never enter the repository.

#### Artifact consistency — confirmed

The original PE SHA-256 matches `ez2dancer/EZ2Dancer.exe` as extracted from the CHD. `module_address` is `0x4c5e` in the seed config, the map config and the runtime descriptor alike, and `id_ref` and `id_verify` match the values extracted at runtime. The four shards sum to 18+23+25+29 = 95 candidates over 10,920×4 = 43,680 seed3 values, the full space; the merged list holds 95 rows with 95 unique seed triples in ascending order. `maps/` holds 95 maps of 13 rows each with no malformed rows and 95 distinct response sets, in the same format as the existing `hardlock-ez2dj3rd.map`, so `--hardlock-transform-map` loads them as `entries=13`.

#### A defect in the challenge list — confirmed

The statically derived challenge list disagrees with what the runtime asks for. The runtime requests **11** unique challenges against the list's **13**. The first **10 match exactly, including their order**. The runtime's eleventh challenge is **absent from the list**, so none of the 95 candidates can answer it and every run reports `mapped=10:unmapped=1`. The list's three remaining entries are never requested at runtime; one of them has the shape of a main-image address and looks like a false positive.

Seed candidates derive from the descriptor and are independent of the challenge list, so the same 95 seeds only need the map-generation step repeated against the runtime list, which is kept at `cfg/ez2d2m/runtime-challenges.txt`.

#### Judgement result — confirmed

Because the missing challenge is the last request, the first ten still separate the candidates. Running all 95 produced exactly two classes: 94 candidates end in the first round with 2 handshakes, 12 descriptors, 11 transforms and 227 or 229 trace lines, while **`candidate-70` alone** reaches 4 handshakes, 13 descriptors and 240 lines.

`candidate-70` is the only candidate that continues past the first round, reopening `\.\FEnteDev` and performing two more handshakes and another descriptor — that is, **the only one to enter a second protection round**. A re-run reproduces the same figures. It is the candidate the artifact README predicted from the product-family B pair of the five previously judged products.

The full sweep is at `cfg/ez2d2m/judgement-sweep.txt`.

#### What this does not mean — unresolved

`candidate-70` still dies once with `0xc0000005`, at line 126 of its 240, immediately after the last transform. The only difference is that execution continues afterwards and reaches a second round.

No candidate opened any asset. `candidate-70` **cannot be declared the correct map**; what is established is only that it alone progresses further. Whether the crash at line 126 results from answering the missing eleventh challenge wrongly is not established — the position makes it plausible, nothing more. Correcting the challenge list, regenerating the maps and re-judging is the next step.

---

## 2026-09-10 런타임 challenge map 재판별 / Re-judgement with runtime-challenge maps

### 한국어

사용자가 런타임 challenge 목록으로 map을 재생성해 `cfg/ez2d2m/runtime-maps/`에 두었습니다. 앞 절에서 지적한 challenge 목록 결함이 해소됐습니다.

#### 산출물 — 확인됨

map 95개는 각 11행이고, challenge 집합이 `runtime-challenges.txt`와 해시 단위로 일치하며, response 조합 95개가 모두 다릅니다. 표본 후보 4개에서 구 정적 map과 공통인 challenge 10개의 response가 완전히 동일해, 같은 seed에서 재생성됐음이 확인됩니다. 실행 시 `entries=11`로 로드되고 `mapped=11:unmapped=0`이 되어 이전의 `unmapped=1`이 사라졌습니다.

#### 재판별 — 확인됨

| 부류 | 후보 수 | handshake | descriptor | trace 줄 |
| --- | --- | --- | --- | --- |
| 1차 라운드에서 종료 | 94 | 2 | 12 | 227 또는 229 |
| **`candidate-70`** | **1** | **4** | **14** | **248** |

분리는 이전과 같고, `candidate-70`은 더 멀리 갑니다. 정적 map일 때 descriptor 13건·240줄이던 것이 descriptor 14건·248줄이 됐습니다. 빠졌던 challenge를 채운 효과가 이 후보에서만 나타납니다.

#### `candidate-70`이 보호를 통과합니다 — 확인됨

`candidate-70`의 실행은 두 번 fault합니다.

| 위치 | 코드 | RVA | 구역 |
| --- | --- | --- | --- |
| 126번째 줄 | `0xc0000005` | `0x00439f1b` | `.protect` |
| 243번째 줄 | `0xc0000096` | `0x0000b565` | **`.text`** |

두 번째 fault의 `0xc0000096`은 privileged instruction이고, 주소가 원본 `.text`(`0x1000`–`0x4c93e`) 안입니다. **게스트가 보호 계층을 지나 원본 게임 코드에서 실행 중이라는 뜻입니다.** 잘못된 map은 여기에 도달할 수 없습니다.

이는 3rd·4th·1st SE·1st·5th에서 확정 후보가 처음 보인 것과 같은 양상입니다. 그 다섯은 트랩되지 않은 byte I/O에서 멈췄고, 여기서는 트랩되지 않은 **word I/O**에서 멈춥니다.

#### 멈춘 지점 — 확인됨

crash context의 code window를 디코드한 결과입니다. 두 번 실행에서 동일합니다.

| 항목 | 값 |
| --- | --- |
| 명령 바이트 | `66 ef` |
| 명령 | `OUT DX, AX` (operand-size prefix `0x66`) |
| `edx` | `0x030a` |
| `eax` | `0x00000004` |
| 앞 명령 | `66 8b c7` = `mov ax, di` |

세부 내용은 [EZ2Dancer I/O 포트 맵](ez2dancer-io-map.md)에 반영했습니다.

#### 아직 아닌 것 — 미확정

- 자산을 아직 열지 못합니다(`asset-open` 0건). 게스트는 자산 로딩 전에 캐비닛 출력을 먼저 건드리고 거기서 멈춥니다.
- `candidate-70`을 정답 map으로 **확정하지는 않습니다.** 원본 `.text` 도달은 매우 강한 증거지만, 이 저장소의 확정 기준은 게스트가 자기 자산을 읽는 것입니다. 그것은 16비트 I/O 경계가 생긴 뒤에만 확인할 수 있습니다.
- 126번째 줄 `.protect` 안의 `0xc0000005`는 실행이 이어지므로 치명적이지 않습니다. 성격은 미확정입니다.

### English

The repository owner regenerated the maps against the runtime challenge list, placing them in `cfg/ez2d2m/runtime-maps/`. The challenge-list defect noted in the previous section is resolved.

#### The artifacts — confirmed

The 95 maps carry 11 rows each, their challenge set matches `runtime-challenges.txt` hash for hash, and all 95 response sets differ. In four sampled candidates the ten challenges shared with the older static maps carry identical responses, confirming regeneration from the same seeds. Runs load them as `entries=11` and report `mapped=11:unmapped=0`, so the former `unmapped=1` is gone.

#### Re-judgement — confirmed

The separation is unchanged and `candidate-70` goes further. Ninety-four candidates still end in the first round with 2 handshakes, 12 descriptors and 227 or 229 lines, while `candidate-70` alone now reaches 4 handshakes, **14** descriptors and **248** lines, against 13 and 240 with the static maps. Filling the missing challenge helped only this candidate.

#### `candidate-70` passes the protection — confirmed

Its run faults twice: `0xc0000005` at RVA `0x00439f1b`, inside `.protect`, at line 126; and `0xc0000096` at RVA `0x0000b565` at line 243. The second code is a privileged instruction and the address lies inside the original `.text` (`0x1000`–`0x4c93e`). **The guest is therefore executing original game code, past the protection layer** — a wrong map cannot get there.

This is the same shape the confirmed candidate first showed on 3rd, 4th, 1st SE, 1st and 5th. Those five stopped on untrapped byte I/O; this one stops on untrapped **word** I/O.

#### Where it stops — confirmed

Decoding the crash context's code window, identically across two runs: the faulting bytes are `66 ef`, which is `OUT DX, AX` with the `0x66` operand-size prefix, with `edx` = `0x030a` and `eax` = `0x00000004`, preceded by `66 8b c7` (`mov ax, di`). The consequences are recorded in [the EZ2Dancer I/O port map](ez2dancer-io-map.md).

#### What this is not — unresolved

No asset is opened yet: the guest touches cabinet output before asset loading and stops there. `candidate-70` is **not declared the resolved map**: reaching original `.text` is strong evidence, but this repository's bar is the guest reading its own assets, and that can only be checked once a 16-bit I/O boundary exists. The `0xc0000005` inside `.protect` at line 126 is not fatal, since execution continues past it; its nature is unresolved.

---

## 2026-09-10 `candidate-70` 확정 / `candidate-70` adopted as resolved

### 한국어

저장소 소유자가 `candidate-70`을 `ez2d2m`의 정답 map으로 확정했습니다. 확정 자료는 3rd·4th·1st·5th와 같은 형태로 배치했습니다.

| 경로 | 내용 |
| --- | --- |
| `cfg/hardlock-ez2d2m.map` | 확정된 challenge-response map 11행 |
| `cfg/hardlock.ini` `[ez2d2m]` | `response450`, `tail44c` |
| `cfg/hardlock-id.ini` `[ez2d2m]` | `module_address`, `id_ref`, `id_verify` |

`cfg/`는 Git이 무시하므로 자료 자체는 저장소에 들어가지 않습니다.

#### 프로파일 기본값만으로 소비됨 — 확인됨

Hardlock 옵션을 하나도 주지 않고 실행하면 launcher가 `hardlock_cfg_material`의 세 항목(`response450`, `tail44c`, `map`)을 모두 인식하고 map을 `entries=11`로 로드합니다. 결과는 명시적 옵션을 준 실행과 같습니다. trace 248줄, handshake 4회, descriptor 14회, transform 11회 전부 매핑(`unmapped=0`), 그리고 원본 `.text`의 RVA `0x0000b565`에서 `0xc0000096`으로 정지합니다.

#### 확정의 근거

- 후보 95개 중 유일하게 분리됩니다. 나머지 94개는 handshake 2회·descriptor 12회에서 1차 라운드를 벗어나지 못합니다.
- 정적 challenge map과 런타임 challenge map 두 세대 모두에서 같은 후보가 분리됐고, 빠진 challenge를 채웠을 때 더 진행한 것도 이 후보뿐입니다.
- 보호 계층을 지나 원본 `.text`를 실행합니다. 정지 지점이 게임 자신의 캐비닛 출력 코드이며, 잘못된 map으로는 도달할 수 없습니다.
- 재현됩니다. 여러 실행에서 같은 수치와 같은 fault 주소·port가 나옵니다.

#### 확정의 한계 — 미확정으로 남는 것

이 저장소가 다른 제품에서 써 온 최종 기준은 게스트가 자기 자산을 읽는 것입니다. `ez2d2m`은 아직 `asset-open` 0건입니다. 게스트가 자산 로딩 전에 캐비닛 출력을 먼저 건드리고 거기서 멈추기 때문이며, 16비트 I/O 경계가 생기기 전에는 그 기준을 적용할 수 없습니다.

따라서 이 확정은 **위 근거에 따른 소유자의 판단**이며, 자산 로딩으로 검증된 것은 아닙니다. 16비트 경계가 생긴 뒤 자산이 열리지 않으면 이 결론을 다시 검토해야 합니다.

### English

The repository owner adopted `candidate-70` as the resolved map for `ez2d2m`, and the material is placed in the same shape as for 3rd, 4th, 1st and 5th: the eleven-row challenge-response map at `cfg/hardlock-ez2d2m.map`, `response450` and `tail44c` in the `[ez2d2m]` section of `cfg/hardlock.ini`, and the descriptor values in `cfg/hardlock-id.ini`. All of `cfg/` is Git-ignored, so none of it enters the repository.

#### Consumed from profile defaults alone — confirmed

Run with no Hardlock option at all, the launcher recognises all three `hardlock_cfg_material` items — `response450`, `tail44c` and `map` — and loads the map as `entries=11`. The result equals the run with explicit options: 248 trace lines, 4 handshakes, 14 descriptors, all 11 transforms mapped with `unmapped=0`, and a stop at `0xc0000096` at RVA `0x0000b565` in the original `.text`.

#### What the adoption rests on

It is the only one of 95 candidates that separates: the other 94 never leave the first round at 2 handshakes and 12 descriptors. The same candidate separated across both generations of maps, static-challenge and runtime-challenge, and it alone went further once the missing challenge was filled. It passes the protection and executes original `.text`, stopping in the game's own cabinet-output code, which a wrong map cannot reach. And it reproduces: repeated runs give the same figures and the same fault address and port.

#### The limit of this adoption — what stays unresolved

The final bar this repository has used for other products is the guest reading its own assets, and `ez2d2m` still shows zero `asset-open`. The guest touches cabinet output before it loads anything and stops there, so that bar cannot be applied until a 16-bit I/O boundary exists.

This adoption is therefore **the owner's judgement on the evidence above**, not a result validated by asset loading. If assets still fail to open once the 16-bit boundary exists, this conclusion has to be revisited.
