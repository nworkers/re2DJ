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

---

## 2026-09-10 word 경계 이후의 정지 지점 / Where it stops once the word boundary exists

### 한국어

[16비트 폭 legacy I/O 경계](../design/20260910-244-word-width-legacy-io.md)가 생긴 뒤의 `ez2d2m` 실행 결과입니다.

#### 확인됨 — 이전 차단 지점은 사라졌습니다

RVA `0x0000b565`의 `0xc0000096`이 없어졌습니다. 게스트는 port `0x030a`에 16비트 쓰기 8회를 수행하고 실행을 이어갑니다. 세부는 [EZ2Dancer I/O 포트 맵](ez2dancer-io-map.md)에 있습니다.

#### 확인됨 — 새 정지 지점

| 항목 | 이전 | 현재 |
| --- | --- | --- |
| 종료 방식 | `0xc0000096` crash | `ExitProcess(0)` |
| 호출 지점 | — | `.protect` RVA `0x0043843a` |
| Hardlock 요청 | 26건 | 31건 (initialize 1, handshake 4, descriptor 15, transform 11) |
| `asset-open` | 0 | 0 |

**게스트는 이제 죽는 대신 스스로 종료합니다.** 종료 호출이 원본 `.text`가 아니라 `.protect` 안에서 나오므로, 결정을 내린 것은 게임이 아니라 보호 계층입니다. 이는 6th에서 관측된 "주 진입 함수가 `-1`을 반환해 종료"와 같은 성격의 경계입니다.

#### 미확정

- 보호 계층이 왜 종료를 선택하는지. `candidate-70` map으로 transform 11건이 모두 응답됐고 descriptor도 15건 완료됐는데도 종료합니다.
- 이것이 `candidate-70`의 오답을 뜻하는지, 아니면 아직 없는 다른 경계(예: 읽지 않은 입력 port, 시간·세션 조건) 때문인지.
- 자산 로딩. 여전히 `asset-open` 0건이므로 [`candidate-70` 확정](#2026-09-10-candidate-70-확정--candidate-70-adopted-as-resolved)에 적어 둔 최종 검증 기준은 아직 충족되지 않았습니다.

### English

This is what an `ez2d2m` run does now that [the word-width legacy I/O boundary](../design/20260910-244-word-width-legacy-io.md) exists.

#### Confirmed — the previous blocker is gone

The `0xc0000096` at RVA `0x0000b565` no longer occurs. The guest performs eight 16-bit writes to port `0x030a` and carries on; the detail is in [the EZ2Dancer I/O port map](ez2dancer-io-map.md).

#### Confirmed — the new stopping point

Where the run used to end in a `0xc0000096` crash, it now ends with `ExitProcess(0)` called from RVA `0x0043843a` inside `.protect`, and Hardlock traffic grows from 26 requests to 31: 1 initialize, 4 handshakes, 15 descriptors and 11 transforms. `asset-open` is still zero.

**The guest now exits deliberately rather than dying.** Because the exit call comes from inside `.protect` and not from the original `.text`, it is the protection layer making that decision, not the game. This is the same kind of boundary as 6th's observed "main entry returns `-1` and exits".

#### Unresolved

Why the protection chooses to exit: all 11 transforms were answered from the `candidate-70` map and 15 descriptors completed, and it exits anyway. Whether that means `candidate-70` is wrong, or whether some other boundary that does not exist yet is responsible — an input port never read, or a timing or session condition. And asset loading: `asset-open` is still zero, so the final validation criterion recorded under [the `candidate-70` adoption](#2026-09-10-candidate-70-확정--candidate-70-adopted-as-resolved) is still unmet.

---

## 2026-09-10 종료 원인: Hardlock Function 0x0001 / Why it exits: Hardlock Function 0x0001

### 한국어

[word 경계 이후의 정지 지점](#2026-09-10-word-경계-이후의-정지-지점--where-it-stops-once-the-word-boundary-exists)에서 미확정으로 남긴 종료 원인을 추적한 결과입니다.

#### 확인됨 — 종료 직전의 순서

한 실행의 요청 순서는 두 라운드로 나뉩니다.

| 라운드 | 요청 |
| --- | --- |
| 1 | initialize 1, handshake 2, descriptor(function `0x0000`) 1, 그 뒤 descriptor(function `0x0006`)와 transform 11쌍 |
| — | `.protect` RVA `0x00439f1b`에서 `0xc0000005` 1회. 실행은 계속됨 |
| 2 | 장치 재개방, handshake 2, descriptor(function `0x0000`) 1 |
| — | port `0x030a`에 16비트 쓰기 8회 (램프 순서열) |
| 2 | descriptor(function `0x0001`) 2회 |
| — | `ExitProcess(0)`, `.protect` RVA `0x0043843a` |

**종료는 Function `0x0001` descriptor 요청 두 건 바로 뒤에 옵니다.** 두 번째 요청은 `data_address=0x001e0000`을 실어 보냅니다. 첫 번째는 `data_address=0x00000000`입니다. 두 요청 모두 `block_count=0`입니다.

#### 확인됨 — 현재 HLE는 Function 0x0001을 구현하지 않습니다

`HardlockDevice`는 descriptor 요청을 입력 그대로 출력에 복사하고 status word를 0으로 지웁니다. `descriptor_tail_word`는 Function `0x0000`에만 적용됩니다. **Function `0x0001`에 대한 처리는 없고, `data_address`는 진단 출력용으로 파싱될 뿐 어디에서도 사용되지 않습니다.** 따라서 보호 계층은 "성공"을 뜻하는 status와 함께 자기가 보낸 요청을 그대로 돌려받습니다.

#### 확인됨 — 종료는 그 응답에 달려 있습니다

인과성을 확인하려고 임시 진단 빌드에서 Function `0x0001` 요청만 거부하고(`rejected-shape`) 나머지는 그대로 두었습니다.

| | 정상 빌드 (echo 응답) | 진단 빌드 (0x0001 거부) |
| --- | --- | --- |
| trace 줄 | 256 | 270 |
| 요청 합계 | 31 | 35 |
| handshake | 4 | 6 |
| descriptor | 15 | 17 |
| rejected | 0 | 4 |
| 종료 지점 | `.protect` `0x0043843a` | 같음 |

거부하면 보호 계층이 **재시도**합니다. 라운드 2에서 handshake 뒤 `0x0001`이 거부되면 다시 handshake를 하고 `0x0001`을 재요청하기를 네 번 반복한 뒤 종료합니다.

**따라서 보호 계층은 Function `0x0001`의 응답을 실제로 검사합니다.** 거부하면 재시도하고, echo를 주면 재시도 없이 즉시 종료합니다. 즉 echo는 형식상 통과하지만 내용이 틀린 것으로 판정됩니다. 진단 패치는 관측 뒤 되돌렸고, 복원한 빌드는 256줄·31요청으로 원래 값과 일치합니다.

#### 확인됨 — 6th와 같은 경계

[ez2dj6th Hardlock 분석](ez2dj6th-hardlock.md)은 6th의 두 descriptor 요청이 function `0x0000`과 `0x0001`이며 최종적으로 주 진입 함수가 `-1`을 반환해 종료한다고 기록합니다. **두 제품이 같은 미구현 요청에서 멈춥니다.**

#### 미확정

- Function `0x0001`이 무엇을 요구하는지. `data_address`가 가리키는 게스트 버퍼에 동글 메모리를 써 주는 요청인지, 256바이트 descriptor 안에 응답을 담는 요청인지 확인되지 않았습니다.
- 유효한 응답 내용. 이 저장소는 보호 응답을 추측해 만들지 않습니다.
- 첫 라운드의 `0xc0000005`가 이 경계와 관련이 있는지.
- 이 결과가 `candidate-70`의 정오와 무관한지. transform 11건은 모두 응답됐고 종료 판정은 그 뒤 별도 단계에서 내려지므로 무관해 보이지만, 확정하려면 유효한 `0x0001` 응답이 필요합니다.

### English

This traces the exit cause left unresolved under [where it stops once the word boundary exists](#2026-09-10-word-경계-이후의-정지-지점--where-it-stops-once-the-word-boundary-exists).

#### Confirmed — the sequence before the exit

A run falls into two rounds. Round one is one initialize, two handshakes, one Function `0x0000` descriptor and then eleven Function `0x0006` descriptor-and-transform pairs, after which a single `0xc0000005` occurs at `.protect` RVA `0x00439f1b` without stopping execution. Round two reopens the device, does two handshakes and one Function `0x0000` descriptor, then the guest writes the eight-step lamp sequence to port `0x030a`, then issues **two Function `0x0001` descriptor requests**, and then calls `ExitProcess(0)` from `.protect` RVA `0x0043843a`.

The first `0x0001` request carries `data_address=0x00000000` and the second `data_address=0x001e0000`; both have `block_count=0`.

#### Confirmed — the current HLE does not implement Function 0x0001

`HardlockDevice` copies a descriptor request straight to the output and clears the status word, and its `descriptor_tail_word` applies to Function `0x0000` only. **There is no handling for Function `0x0001`, and `data_address` is parsed for the diagnostic line but used nowhere.** The protection therefore gets its own request back, accompanied by a status word meaning success.

#### Confirmed — the exit depends on that answer

To test causality, a temporary diagnostic build refused Function `0x0001` requests alone, leaving everything else as it was. The normal build produces 256 trace lines and 31 requests with 4 handshakes, 15 descriptors and nothing rejected; the diagnostic build produces 270 lines and 35 requests with 6 handshakes, 17 descriptors and 4 rejections, and exits at the same address.

Refusing makes the protection **retry**: in round two, when `0x0001` is refused it handshakes again and re-requests `0x0001`, four times over, before giving up and exiting.

**The protection therefore does inspect the Function `0x0001` answer.** Refused, it retries; echoed, it exits at once without retrying — so the echo passes the shape check and fails on content. The diagnostic patch was reverted after the observation, and the restored build reproduces the original 256 lines and 31 requests exactly.

#### Confirmed — the same boundary as 6th

[The ez2dj6th Hardlock analysis](ez2dj6th-hardlock.md) records that 6th's two descriptor requests are Function `0x0000` and `0x0001`, and that its main entry ultimately returns `-1` and exits. **Two products stop at the same unimplemented request.**

#### Unresolved

What Function `0x0001` asks for — whether it writes dongle memory into the guest buffer at `data_address` or returns its answer inside the 256-byte descriptor. What a valid answer contains; this repository does not invent protection responses. Whether the round-one `0xc0000005` relates to this boundary. And whether this result is independent of whether `candidate-70` is correct: all eleven transforms were answered and the exit decision is taken in a later, separate step, which suggests it is, but settling that needs a valid `0x0001` answer.

---

## 2026-09-10 종료 원인 정정: Function 0x0011 / Corrected exit cause: Function 0x0011

### 한국어

앞 절 [종료 원인: Hardlock Function 0x0001](#2026-09-10-종료-원인-hardlock-function-0x0001--why-it-exits-hardlock-function-0x0001)의 결론은 **틀렸습니다.** 두 가지를 정정합니다.

1. `ExitProcess(0)`은 오류 종료가 아니라 **명시적 정상 종료**입니다.
2. Function `0x0001`은 데이터 조회가 아니라 **API teardown**(레거시 Hardlock API의 `API_DOWN`)이며, 종료를 **결정하는** 단계가 아니라 종료 **경로에 있는** 단계입니다.

앞 절의 거부 실험은 "보호 계층이 `0x0001`의 성공 여부를 신경 쓴다"는 것만 보였을 뿐, 응답 내용이 거절됐다는 근거가 되지 못합니다. 순서상 앞에 있다는 사실과 원인이라는 사실을 제가 충분히 갈라내지 못했습니다.

#### 실제 원인 — 확인됨

관측을 가로막던 두 번째 문제가 있었습니다. 게스트가 자기 `EZ2Dancer.ini`를 열 때 `error=2`로 실패하고 있었습니다. 원인은 runtime의 `ChdRelativePath`가 이미지 내부 디렉터리를 **`"EZ2DJ/"`로 하드코딩**한 것입니다. `ez2d2m`의 디렉터리는 `ez2dancer`이므로 CHD fallback이 언제나 빗나갔습니다. 이 저장소 첫 비-EZ2DJ 제품이 드러낸 가정입니다.

INI가 열리자 게스트가 한 걸음 더 갑니다. transform 요청이 11건에서 12건으로 늘고, **12번째 요청은 `function=0x0011`에 `block_count=7`입니다.** 앞의 11건은 모두 `function=0x000e`의 단일 블록이고 전부 매핑되지만, 이 7블록은 **7개 모두 unmapped**입니다.

| 요청 | function | block_count | 매핑 |
| --- | --- | --- | --- |
| 1–11 | `0x000e` | 1 | 11건 전부 매핑 |
| 12 | `0x0011` | 7 | **0 매핑 / 7 unmapped** |

**따라서 종료 원인은 응답할 수 없는 `function=0x0011` 7블록 transform입니다.** 그 뒤에 오는 `0x0001` 두 건은 teardown이고, `ExitProcess(0)`은 그 결과입니다.

#### 확인됨 — 6th와 같은 미해결 요청

[ez2dj6th Hardlock 분석](ez2dj6th-hardlock.md)은 6th의 미해결 항목을 "Function `0x0011` 응답"으로, 그 입력을 7개로 기록합니다. **두 제품이 같은 미해결 요청에서 멈춥니다.**

#### 확인됨 — 7블록 입력의 모양

입력 7블록은 `cfg/ez2d2m/runtime-challenges-full.txt`에 있습니다. 앞 블록들은 0과 `cc` 채움이 섞여 있고 뒤 두 블록만 값처럼 보입니다. `cc`는 초기화되지 않은 스택/힙 채움 패턴이므로, 이 요청이 부분적으로만 채워진 구조체를 그대로 보낸다는 뜻일 수 있습니다. 이 해석은 **추정**입니다.

#### 미확정

- `function=0x0011`이 무엇을 계산하는지와 유효한 7블록 응답. 이 저장소는 보호 응답을 추측해 만들지 않습니다.
- `0x0001` 응답이 teardown으로서 충분한지. `0x0011`을 넘긴 뒤에야 확인할 수 있습니다.
- 7블록 중 `cc` 채움이 실제로 초기화되지 않은 메모리인지.

### English

The conclusion of the previous section, [why it exits: Hardlock Function 0x0001](#2026-09-10-종료-원인-hardlock-function-0x0001--why-it-exits-hardlock-function-0x0001), is **wrong**, on two counts. `ExitProcess(0)` is a deliberate normal exit, not an error exit; and Function `0x0001` is not a data query but the legacy Hardlock API's `API_DOWN` teardown — a step on the way out rather than the step that decides to leave.

The refusal experiment there showed only that the protection cares whether `0x0001` succeeds. It is not evidence that its content was rejected, and I did not separate "precedes the exit" from "causes it" well enough.

#### The real cause — confirmed

A second problem was hiding the first: the guest's own `EZ2Dancer.ini` open was failing with `error=2`, because the runtime's `ChdRelativePath` hard-coded the product's directory inside the image as **`"EZ2DJ/"`**. `ez2d2m` lives under `ez2dancer`, so every CHD fallback for it missed — an assumption this repository's first non-EZ2DJ product exposed.

With the INI readable the guest goes one step further. Its transform requests rise from eleven to twelve, and **the twelfth is `function=0x0011` with `block_count=7`**. The first eleven are single-block `function=0x000e` requests and all map; the seven blocks of the twelfth are **all unmapped**.

**The exit cause is therefore the unanswerable `function=0x0011` seven-block transform.** The two `0x0001` requests that follow are teardown, and `ExitProcess(0)` is its consequence.

#### Confirmed — the same unresolved request as 6th

[The ez2dj6th Hardlock analysis](ez2dj6th-hardlock.md) records 6th's unresolved item as the Function `0x0011` response and its input as seven blocks. **Two products stop at the same unresolved request.**

#### Confirmed — the shape of the seven-block input

The seven input blocks are in `cfg/ez2d2m/runtime-challenges-full.txt`. The leading blocks mix zeros with `cc` filler and only the last two look like values. Since `cc` is an uninitialised stack or heap fill pattern, the request may be sending a partly-filled structure as-is; that reading is **inferred**.

#### Unresolved

What `function=0x0011` computes and what a valid seven-block answer contains — this repository does not invent protection responses. Whether the `0x0001` answer suffices as teardown, which can only be checked past `0x0011`. And whether the `cc` filler really is uninitialised memory.

---

## 2026-09-11 Function 0x0011 입력의 구조 / The structure of the Function 0x0011 input

### 한국어

두 번째 머신(`E:\MYWORK\Projects\re2DJ`)에서 같은 CHD로 실행한 결과입니다. 절차와 코드 변경은 [작업 247](../work-logs/20260911-247-hardlock-payload-response-rows.md)에 있습니다.

#### 확인됨 — 다른 머신에서도 같은 정지 지점

제품 경로 `re2dj ez2d2m` 실행은 trace 261줄, 264바이트 transform 11건 전부 매핑, 312바이트 transform 1건 `unmapped=7`, 이후 `ExitProcess(0)`입니다. [종료 원인 정정 절](#2026-09-10-종료-원인-정정-function-0x0011--corrected-exit-cause-function-0x0011)과 같습니다.

#### 확인됨 — 7블록 입력은 결정적이지 않습니다

`--hardlock-transform-input-dump`로 두 번 받고, 이전 머신의 관찰(reSoftlock `artifacts/ez2d2m/runtime-challenges-full.txt`)과 대조했습니다. 바이트는 기록하지 않습니다.

| 블록 | 이 머신 두 실행 | 이전 머신과 | 관찰 |
| --- | --- | --- | --- |
| `0x000e` 11건 | 같음 | 같음 | 결정적 |
| 0 | 다름 | 다름 | DWORD 하나가 두 실행의 runtime 적재 주소 차이 `0x50000`만큼 이동. 나머지는 `cc` |
| 1 | 다름 | 다름 | 블록 0과 같은 이동. 나머지는 `cc` |
| 2 | 같음 | 다름 | 이 머신은 전부 0, 이전 머신은 절반이 `cc` |
| 3 | 같음 | 다름 | 첫 DWORD는 같고, 둘째 DWORD의 상위 16비트만 다름 |
| 4 | 같음 | 다름 | 0과 `cc` 채움이 머신마다 다름 |
| 5 | 같음 | 같음 | **세 관찰 모두 동일** |
| 6 | 다름 | 다름 | 블록 0과 같은 이동 |

블록 0·1·6의 이동량이 runtime 적재 주소 차이와 정확히 같으므로, 이 값들은 re2DJ runtime 쪽 메모리를 가리키는 포인터 성격의 값입니다. 요청 전체를 정확히 비교하는 응답 행은 다음 실행에서 빗나갑니다.

#### 확인됨 — 상위 계약 대조 (GPL 소스, 구현 근거 아님)

2EZConfig-V2 commit `a7346e066bb569643e0cb25f41cfdc079126fbc6`을 사실 대조에만 썼습니다. 코드는 옮기지 않았습니다. 세부는 [Hardlock API function 코드](../kb/hardlock-api-functions.md)에 있습니다.

- `fastapi.h`: `API_CRYPT 14`(`0x0e`), `API_CODE 17`(`0x11`), `API_INIT 0`, `API_DOWN 1`, `API_AVAIL 6`.
- `API_CODE` 처리: 끝에서 두 번째 블록을 입력으로 한 번 계산하고, 결과를 블록 0–2와 4–5에 쓰며 블록 3의 두 DWORD에는 더하고, 마지막 블록은 유지합니다.

#### 추정

- `ez2d2m`의 `function=0x0011`은 `API_CODE`입니다. 상위 계약의 계산 입력 위치(7블록 요청의 블록 5)가 정확히 모든 관찰에서 변하지 않는 블록이고, descriptor function `0x0000`·`0x0001`·`0x0006`도 `API_INIT`·`API_DOWN`·`API_AVAIL`과 맞습니다.
- 블록 0–4와 6은 게스트가 채우지 않은 버퍼 내용입니다. 앞 절에서 추정으로 남긴 `cc` 해석과 맞습니다.
- 따라서 유효한 응답은 블록 5(와 더하기 대상인 블록 3)에만 의존하며, 나머지 입력 바이트는 응답 행에서 지정하지 않아야 합니다.

#### 이 저장소가 한 것

응답 표에 `??`를 허용하는 **요청 행**을 추가했습니다([설계 247](../design/20260911-247-hardlock-payload-response-rows.md)). 블록 5만 지정하고 그 값을 그대로 되돌려 쓰는 **항등** 요청 행으로 전달 경로를 시험했고, 적재 주소가 다른 새 실행에서도 12번째 transform이 `payload=1`로 맞았습니다. 항등 행은 새 응답을 만들지 않으므로 게스트 동작은 그대로(261줄, 종료)입니다.

#### 미확정

- 유효한 `API_CODE` 응답. 외부 도구가 요청 행으로 만들어야 합니다. 현재 reSoftlock은 `0x0011`에 블록별 `HL_CRYPT`를 쓰는 가설 경로만 갖고 있어, 위 계약과 모양이 맞지 않습니다.
- 게스트가 `API_CODE` 응답의 어느 바이트를 검사하는지.
- 블록 3 둘째 DWORD의 상위 16비트가 머신마다 다른 이유.

### English

This is the same CHD run on a second machine (`E:\MYWORK\Projects\re2DJ`). The procedure and code change are in [task 247](../work-logs/20260911-247-hardlock-payload-response-rows.md).

#### Confirmed — the same stopping point on another machine

The product path `re2dj ez2d2m` gives 261 trace lines, all eleven 264-byte transforms mapped, one 312-byte transform with `unmapped=7`, then `ExitProcess(0)` — the same as [the corrected exit-cause section](#2026-09-10-종료-원인-정정-function-0x0011--corrected-exit-cause-function-0x0011).

#### Confirmed — the seven-block input is not deterministic

The input was captured twice with `--hardlock-transform-input-dump` and compared with the other machine's capture (reSoftlock `artifacts/ez2d2m/runtime-challenges-full.txt`); no byte is recorded here. The eleven `0x000e` challenges are identical everywhere. In the `0x0011` request, blocks 0, 1 and 6 differ between the two runs here, each with one DWORD that moves by `0x50000` — exactly the difference between the runs' runtime load addresses — and `cc` elsewhere. Blocks 2 to 4 are identical within this machine but differ from the other: block 2 is all zero here and half `cc` there, block 3 shares its first DWORD and differs only in the upper 16 bits of the second, and block 4's zero and `cc` fill differs. **Block 5 is identical in all three captures.**

Because blocks 0, 1 and 6 move by exactly the load-address difference, they hold pointer-like values into re2DJ's runtime memory. A response row comparing the whole request exactly would miss on the next run.

#### Confirmed — upstream contract comparison (GPL source, not an implementation basis)

2EZConfig-V2 at commit `a7346e066bb569643e0cb25f41cfdc079126fbc6` was used for fact comparison only, and no code was carried over; details are in [Hardlock API function codes](../kb/hardlock-api-functions.md). Its `fastapi.h` defines `API_CRYPT 14` (`0x0e`), `API_CODE 17` (`0x11`), `API_INIT 0`, `API_DOWN 1` and `API_AVAIL 6`. Its `API_CODE` handling computes once from the second-to-last block, writes the result to blocks 0–2 and 4–5, adds into the two DWORDs of block 3, and keeps the last block.

#### Inferred

`ez2d2m`'s `function=0x0011` is `API_CODE`: the contract's input position, block 5 of a seven-block request, is exactly the block that never changes across captures, and the descriptor functions `0x0000`, `0x0001` and `0x0006` likewise match `API_INIT`, `API_DOWN` and `API_AVAIL`. Blocks 0–4 and 6 are buffer contents the guest never filled, which agrees with the `cc` reading left as inferred in the previous section. A valid answer therefore depends only on block 5, and on block 3 as the addition target, and a response row should leave the other input bytes unspecified.

#### What this repository did

The response table gained **request rows** that allow `??` ([design 247](../design/20260911-247-hardlock-payload-response-rows.md)). The transfer path was tested with an **identity** request row that specifies only block 5 and writes it back unchanged: on a fresh run with a different load address the twelfth transform matched with `payload=1`. The identity row invents no answer, so the guest behaves as before — 261 lines, then exit.

#### Unresolved

A valid `API_CODE` answer, which an external tool must produce as a request row; reSoftlock currently has only a hypothesis path that applies per-block `HL_CRYPT` to `0x0011`, a shape that does not match the contract above. Which bytes of the `API_CODE` answer the guest checks. And why the upper 16 bits of block 3's second DWORD differ between machines.

---

## 2026-09-11 API_CODE 응답을 넣어도 진행하지 않음 / Answering API_CODE does not advance

### 한국어

reSoftlock에 `HL_CODE` 기반 `code-map`이 생긴 뒤([reSoftlock 작업 006](../../../reSoftlock/docs/work-logs/20260911-006-ez2d2m-api-code-response.md)), candidate-70 seed로 `0x0011` 요청 행을 만들어 기존 11개 블록 행 뒤에 붙여 실행했습니다.

#### 확인됨 — 요청 행은 적용되지만 결과가 같습니다

| 항목 | 응답 없음(echo) | HL_CODE 요청 행 |
| --- | --- | --- |
| 12번째 transform | `unmapped=7:payload=0` | `unmapped=0:payload=1` |
| trace 줄 | 261 | 261 |
| 종료 | `.protect` `0xc0000005` 뒤 `ExitProcess` | 동일 |
| asset-open | 0 | 0 |

요청 행이 정확히 적용되어 12번째 transform이 응답됐는데도(`payload=1`), 실행은 응답 없음과 **바이트 단위로 같습니다.**

#### 확인됨 — 블록 0·1·2·4·5 내용은 관문이 아닙니다

echo 실행과 HL_CODE 실행은 블록 0·1·2·4·5의 바이트가 서로 다른데도 downstream이 완전히 같습니다. 따라서 게스트는 이 블록들의 내용으로 분기하지 않습니다. 변화를 주지 않은 것은 블록 3(emulator의 32비트 LE `+=` add)과 블록 6(유지)뿐입니다.

#### 미확정

- 블록 3의 add가 관문인지. 현재 요청 행은 쓰기/유지만 표현해 add를 담지 못합니다. add가 관문이면 re2DJ에 carry-aware add 연산을 넣어 블록 3을 `input + crypt`로 만들어야 합니다.
- 아니면 종료가 `0x0011` 응답과 무관한지. 블록 0·1·2·4·5가 무관하다는 관측은 "응답 내용 자체가 관문이 아니다"와도 부합합니다. 이 경우 관문은 teardown `0x0001`, 세션·시간 조건, 또는 아직 읽지 않은 입력 port입니다.
- 값싼 다음 실험: `0x0011` 요청만 거부해 게스트가 재시도하는지 관찰. 재시도하면 응답을 검사한다는 뜻이고, add 연산 투자가 정당화됩니다.

### English

After reSoftlock gained the `HL_CODE`-based `code-map` ([reSoftlock work log 006](../../../reSoftlock/docs/work-logs/20260911-006-ez2d2m-api-code-response.md)), a `0x0011` request row was generated from candidate-70's seeds and appended to the existing eleven block rows.

**Confirmed — the row applies but the result is unchanged.** The twelfth transform now reports `unmapped=0:payload=1` instead of `unmapped=7:payload=0`, yet the run is byte-identical to the no-answer run: 261 lines, the same `0xc0000005` in `.protect` then `ExitProcess`, and zero asset opens.

**Confirmed — blocks 0/1/2/4/5 content is not the gate.** The echo run and the `HL_CODE` run carry different bytes in those blocks with identical downstream, so the guest does not branch on them. The only outputs left unvaried are block 3 (the emulator's 32-bit little-endian `+=` add) and block 6 (kept).

**Unresolved.** Whether block 3's add is the gate — the current request row expresses only write and keep, so if the add gates the exit, re2DJ needs a carry-aware add making block 3 `input + crypt`. Or whether the exit is independent of the `0x0011` answer, which the blocks-0/1/2/4/5 observation fits equally, leaving the `0x0001` teardown, a session or timing condition, or an unread input port as the gate. The cheap next experiment is to reject the `0x0011` request and see whether the guest retries; a retry would show it inspects the answer and justify the add operation.

#### 확인됨 — `0x0011` 거부 실험 (2026-09-11)

launcher에 진단 옵션 `--hardlock-reject-function <hex>`를 추가했습니다. 지정한 function의 descriptor·transform을 완료하지 않고 `rejected-shape`로 되돌립니다. 프로파일 기본값이 아니며 명시적 옵션으로만 켜집니다.

candidate-70 블록 행 map에 `--hardlock-reject-function 0x0011`을 얹어 실행한 결과입니다.

| 항목 | 완료(echo 또는 HL_CODE) | `0x0011` 거부 |
| --- | --- | --- |
| `0x0011` transform | 1건 completed | **2건 rejected-shape (1회 재시도)** |
| transform 합계 | 12 | 13 |
| handshake·descriptor | 4·15 | 4·15 (동일) |
| trace 줄 | 261 | 266 |
| 종료 | `.protect` `0x439f1b` `0xc0000005` 뒤 `ExitProcess` | 동일 |

**게스트는 `0x0011` 결과를 검사합니다.** IOCTL이 실패하면 한 번 재시도한 뒤 종료하고, 성공하면 재시도 없이 종료합니다. 즉 `0x0011`은 no-op이 아니며 답이 필요합니다.

다만 앞 절과 합치면 검사의 성격이 좁혀집니다. 게스트는 **IOCTL의 성공 여부**를 검사하지, 내용은 구분하지 않습니다. echo(확실히 틀린 내용)와 HL_CODE(더 맞는 내용)가 똑같이 "성공"으로 받아들여지고 downstream이 같기 때문입니다. echo만으로도 성공 조건은 이미 충족됩니다.

#### 미확정 — 좁혀진 상태

- **블록 3의 add만 검증되지 않았습니다.** echo와 HL_CODE 모두 블록 3을 게스트 입력 그대로 두므로, 블록 3을 `input + crypt`로 채우는 실험은 아직 못 했습니다. 이것이 유일하게 남은 내용 레버입니다.
- 다만 내용을 "더 맞게"(HL_CODE) 만들어도 변화가 없었다는 점은, 종료가 `0x0011` 내용과 무관할 가능성(teardown `0x0001`, 1라운드의 `0x439f1b` `0xc0000005`, 세션·시간 조건, 미독 입력 port)에 무게를 싣습니다.
- 블록 3 add를 검증하려면 re2DJ 요청 행에 carry-aware 32비트 add 연산이 필요합니다. 그 전에는 확정할 수 없습니다.

### English

#### Confirmed — the `0x0011` rejection experiment (2026-09-11)

The launcher gained a diagnostic option `--hardlock-reject-function <hex>` that returns `rejected-shape` for a chosen function's descriptor and transform instead of completing it. It is never a profile default and is enabled only by the explicit option.

Running the candidate-70 block-row map with `--hardlock-reject-function 0x0011`: where a completed answer (echo or `HL_CODE`) produces one `0x0011` transform and 261 lines, rejecting it produces **two** `0x0011` transforms — one retry — and 266 lines, with handshakes and descriptors unchanged at 4 and 15 and the same `0xc0000005` at `.protect` `0x439f1b` then `ExitProcess`.

**The guest inspects the `0x0011` result:** it retries once when the IOCTL fails and does not retry when it succeeds, so `0x0011` is not a no-op and an answer is needed. But combined with the previous section this narrows what the check is: the guest checks **whether the IOCTL succeeded**, not its content, since echo (definitely wrong) and `HL_CODE` (more nearly right) are both accepted as success with identical downstream, and echo alone already satisfies the success condition.

#### Unresolved — the narrowed state

Only block 3's add is untested: echo and `HL_CODE` both leave block 3 at the guest's input, so writing block 3 as `input + crypt` has not been tried, and it is the only remaining content lever. That making the content more nearly correct (`HL_CODE`) changed nothing weighs toward the exit being independent of `0x0011` content — the `0x0001` teardown, round one's `0x439f1b` `0xc0000005`, a session or timing condition, or an unread input port. Settling block 3 needs a carry-aware 32-bit add operation in re2DJ's request row; until then it cannot be decided.

---

## 2026-09-11 1라운드 `0xc0000005`은 packer SEH trampoline / The round-one `0xc0000005` is a packer SEH trampoline

### 한국어

종료보다 앞서는 1라운드 `.protect` `0xc0000005`(RVA `0x00439f1b`)가 진짜 원인인지 조사했습니다. 새 코드 없이, 기존 trace의 `crash-exception` 바이트와 `crash-context` code window를 디코드했습니다.

#### 확인됨 — 의도된 fault

fault EIP의 바이트는 `8b0b 0fb9 ...`이고, 그 앞 code window는 `8b1d40a78300`입니다.

| 주소 | 바이트 | 명령 |
| --- | --- | --- |
| `0x439f15` | `8b 1d 40 a7 83 00` | `mov ebx, [0x0083a740]` |
| `0x439f1b` | `8b 0b` | `mov ecx, [ebx]` ← fault |
| `0x439f1d` | `0f b9` | `UD1` (정의되지 않은 명령) |

fault 시 `ebx=0`입니다. 즉 `[0x0083a740]`이 0이라 `mov ecx,[ebx]`가 주소 0을 읽어 access violation이 납니다. **핵심은 그 다음 바이트가 `UD1`이라는 점입니다.** `ebx`가 0이 아니어서 mov가 성공했더라도 바로 다음에서 `#UD`로 fault합니다. 즉 이 지점은 **어느 경로로도 반드시 fault하도록 설계**돼 있습니다.

#### 확인됨 — SEH가 삼키고 실행이 이어집니다

이 fault 뒤로 실행이 계속됩니다. 같은 run이 이후 descriptor 15건과 transform 12건을 마치고 정상적으로 `ExitProcess(0)`에 도달합니다(261줄). detached 실행에는 디버거가 붙지 않으므로 first-chance 예외는 곧바로 게스트의 SEH로 전달됩니다. 실행이 이어진다는 것은 **게스트(packer)가 이 예외를 자기 SEH로 처리한다**는 뜻입니다. 처리기가 없었다면 그 자리에서 프로세스가 죽습니다.

#### 결론 — 종료 원인이 아닙니다

이것은 보호 계층이 제어 흐름·복호화 단계를 SEH로 옮기는 packer trampoline입니다. `[0x0083a740]`이 0인 것은 re2DJ가 채워야 할 미해결 포인터가 아니라 fault를 유발하기 위한 값이며, 바로 뒤의 `UD1`이 그 의도를 확증합니다. 따라서 1라운드 `0xc0000005`는 **종료 원인 후보에서 제외**됩니다. 종료는 뒤 단계의 보호 계층 판단이며, [API_CODE 절](#2026-09-11-api_code-응답을-넣어도-진행하지-않음--answering-api_code-does-not-advance)에서 본 대로 `0x0011` 내용(blocks 0·1·2·4·5)과도 무관합니다.

#### 남은 레버 — 미확정

- candidate-70이 더 깊은 단계에서 정답인지. 보호를 지나 원본 `.text`에 도달한 관측(작업 243)은 CHD 루트 수정과 종료 원인 정정 이전 것이므로 재확인이 필요합니다.
- 블록 3 add(유일한 미검증 내용 레버).
- 아직 읽지 않은 입력 port, 세션·시간 조건 같은 환경 요인.

### English

This checks whether the round-one `.protect` `0xc0000005` at RVA `0x00439f1b`, which precedes the exit, is the real cause. No new code: the existing trace's `crash-exception` bytes and `crash-context` code window were decoded.

**Confirmed — a deliberate fault.** The faulting EIP bytes are `8b0b 0fb9 …`, preceded in the window by `8b1d40a78300`, giving `mov ebx, [0x0083a740]` at `0x439f15`, `mov ecx, [ebx]` at `0x439f1b` (the fault), and `UD1` (`0f b9`) at `0x439f1d`. At the fault `ebx=0`, so `[0x0083a740]` held zero and `mov ecx,[ebx]` read address 0. The decisive point is that the next byte is `UD1`: even if `ebx` had been non-zero and the `mov` had succeeded, the very next instruction raises `#UD`. This location is **designed to fault on either path**.

**Confirmed — an SEH swallows it and execution continues.** Execution proceeds past the fault: the same run completes fifteen descriptors and twelve transforms and reaches `ExitProcess(0)` normally at 261 lines. A detached run has no debugger attached, so first-chance exceptions go straight to the guest's SEH; that execution continues means the guest — the packer — handles this exception in its own SEH, since without a handler the process would die here.

**Conclusion — it is not the exit cause.** This is a packer trampoline that moves control-flow or decryption work into an SEH handler. `[0x0083a740]` being zero is not an unresolved pointer re2DJ should fill but a value chosen to force the fault, and the `UD1` immediately after confirms the intent. The round-one `0xc0000005` is therefore **removed from the exit-cause candidates**. The exit is a later protection decision, and — as the [API_CODE section](#2026-09-11-api_code-응답을-넣어도-진행하지-않음--answering-api_code-does-not-advance) showed — independent of the `0x0011` content in blocks 0/1/2/4/5.

**Remaining levers — unresolved.** Whether candidate-70 is correct at a deeper stage (the observation of reaching original `.text` in task 243 predates the CHD-root fix and the exit-cause correction and needs re-confirming); block 3's add, the one untested content lever; and environment factors such as an unread input port or a session or timing condition.

---

## 2026-09-11 종료는 게임 `.text` 경로에서 일어납니다 / The exit is reached from a game `.text` path

### 한국어

종료 지점 `.protect` `0x43843a`가 무엇을 보고 `ExitProcess`를 부르는지 조사했습니다. 주입 runtime의 exit wrapper에 stack 복귀 주소 스냅샷을 추가했습니다(코드 주소만 기록, 비밀값 없음). launcher 옵션이 아니라 상시 동작하는 진단입니다.

#### 확인됨 — 종료 호출 지점

`exit-process` 기록: `code=0`, `caller_rva=0x0043843a`. 그 앞 24바이트는 `... c3`(0x43842e의 `ret`) 뒤 `ff 15 9c 98 83 00`(`call dword ptr [0x0083989c]`)입니다. 즉 `0x43843a`는 `ret`로 구분된 별도 블록에서 `call [ExitProcess]`을 수행하는 **공용 exit thunk**이며, 다른 코드가 이 thunk를 호출합니다.

#### 확인됨 — 종료 시점의 stack은 게임 `.text` 복귀 주소로 채워져 있습니다

`exit-stack-chain`이 종료 시점 stack의 이미지 내부 복귀 주소를 기록했습니다. `.text`는 RVA `0x1000`–`0x4c93e`, entry stub는 `0x401240`(`.protect`), exit thunk는 `0x43843a`(`.protect`)입니다.

| stack index | RVA | 구역 |
| --- | --- | --- |
| 0 | `0x43843a` | `.protect` (exit thunk) |
| 6 | `0x45ba1` | **`.text`** |
| 8 | `0x401240` | `.protect` (entry stub) |
| 9 | `0x45ae9` | **`.text`** |
| 13 | `0x41a5b` | **`.text`** |
| 18 | `0x408323` | **`.text`** |
| 40 | `0x431188` | **`.text`** |
| 41 | `0x43e28` | **`.text`** |
| 42 | `0x4f180` | `.rdata` |

`.text` 복귀 주소가 여러 개 stack에 올라와 있습니다. **즉 `ExitProcess(0)`에 이르는 호출 사슬이 원본 게임 `.text` 코드를 지나갑니다.** exit thunk를 부른 것은 게임 코드입니다.

#### 결론 — 종료를 결정한 것은 게임입니다

이는 [word 경계 절](#2026-09-10-word-경계-이후의-정지-지점--where-it-stops-once-the-word-boundary-exists)의 "결정을 내린 것은 게임이 아니라 보호 계층"이라는 서술을 **정정**합니다. `0x43843a`는 보호 계층의 exit thunk일 뿐이고, 그것을 호출한 경로는 게임 `.text`입니다. 따라서 candidate-70은 보호를 통과했고, 제어는 게임에 있으며, **게임 자신이 자산을 하나도 열기 전에 스스로 종료**합니다(`asset-open` 0건). 종료 코드는 0(정상)입니다.

이는 [API_CODE 절](#2026-09-11-api_code-응답을-넣어도-진행하지-않음--answering-api_code-does-not-advance)의 관측과 정합적입니다. 종료가 게임 로직에서 결정되므로 `0x0011` 응답 내용과 무관합니다.

#### 미확정 — 이제 게임 로직

- 게임 `.text`의 어느 검사가 종료를 유발하는지. stack의 `0x45ba1`, `0x45ae9`, `0x41a5b`, `0x408323` 부근 코드가 다음 조사 대상입니다.
- 자산 로딩 전에 걸리는 조건. 캐비닛 I/O readback(port `0x030a` 계열 입력), `EZ2Dancer.ini`의 특정 값, 서비스·코인·시간 조건 등이 후보입니다. word 출력 8회는 이미 관측됐지만, 대응하는 **입력** port는 아직 `legacy_io_in_rva=0`으로 트랩되지 않습니다.

### English

This traces what the exit site `.protect` `0x43843a` sees when it calls `ExitProcess`. The injected runtime's exit wrapper gained a stack return-address snapshot (code addresses only, no secrets); it is always-on rather than a launcher option.

**Confirmed — the exit call site.** The `exit-process` record shows `code=0`, `caller_rva=0x0043843a`, and the 24 bytes before it are `… c3` (a `ret` at `0x43842e`) then `ff 15 9c 98 83 00` (`call dword ptr [0x0083989c]`). So `0x43843a` is a **shared exit thunk** — a `ret`-delimited block that does `call [ExitProcess]` — and other code calls it.

**Confirmed — the stack at exit is full of game `.text` return addresses.** With `.text` at RVA `0x1000`–`0x4c93e`, the entry stub at `0x401240` (`.protect`) and the thunk at `0x43843a` (`.protect`), the `exit-stack-chain` records image-resident return addresses at indices 6/9/13/18/40/41 of `0x45ba1`, `0x45ae9`, `0x41a5b`, `0x408323`, `0x431188` and `0x43e28` — all in `.text` — interleaved with the entry stub at 8/15/16/75. **The call chain reaching `ExitProcess(0)` therefore runs through original game `.text` code; game code called the exit thunk.**

**Conclusion — the game decides to exit.** This **corrects** the [word-boundary section](#2026-09-10-word-경계-이후의-정지-지점--where-it-stops-once-the-word-boundary-exists), which said the protection rather than the game made that decision. `0x43843a` is only the protection's exit thunk, and the path that called it is game `.text`. So candidate-70 passed the protection, control is in the game, and **the game exits on its own before opening any asset** (`asset-open` zero), with exit code 0. This is consistent with the [API_CODE section](#2026-09-11-api_code-응답을-넣어도-진행하지-않음--answering-api_code-does-not-advance): the exit is decided in game logic, so it is independent of the `0x0011` answer content.

**Unresolved — now in game logic.** Which check in game `.text` triggers the exit — the code around `0x45ba1`, `0x45ae9`, `0x41a5b`, `0x408323` is the next target — and what condition it fails before loading assets: a cabinet-I/O readback (an input on the `0x030a` port family), a specific `EZ2Dancer.ini` value, or a service/coin/time condition. The eight word writes were observed, but the corresponding **input** port is still untrapped (`legacy_io_in_rva=0`).

---

## 2026-09-11 종료는 게임의 정상 CRT `exit(0)` / The exit is the game's normal CRT `exit(0)`

### 한국어

exit wrapper가 stack의 `.text` 복귀 주소마다 그 앞 코드 창을 덤프하게 하고(코드 주소·바이트만, 비밀값 없음), 종료 직전 trace 꼬리를 함께 읽었습니다.

#### 확인됨 — 종료 직전 순서

한 실행의 마지막 동작은 다음과 같습니다.

1. 장치 재개방, handshake 2회, Function `0x0000` descriptor.
2. `EZ2Dancer.ini` 712바이트를 CHD에서 읽음(성공).
3. 7블록 `function=0x0011` transform.
4. port `0x030a`에 16비트 **쓰기 8회**: `0x0004, 0x0104, 0x0304, 0x0704, 0x0f04, 0x0f05, 0x0f07, 0x0f07`.
5. Function `0x0001`(API_DOWN) descriptor 2회 — dongle 세션 teardown.
6. `ExitProcess(0)`.

#### 확인됨 — 입력 port를 전혀 읽지 않습니다

`io-port:dir=read`가 0건입니다. 게스트는 캐비닛 **출력**만 하고 입력은 한 번도 읽지 않습니다. **따라서 종료는 입력 대기 hang이 아니라 결정적 조기 종료입니다.**

#### 확인됨 — CRT 종료 경로

종료 직전 dynamic-resolver 이름이 `GetACP`, `GetOEMCP`, `GetStringTypeW`, `GetStringTypeA`, `GetTimeZoneInformation`, `HeapDestroy`입니다. 이들은 MSVCRT의 locale·shutdown 함수입니다. C2에서 본 게임 `.text` 프레임과 합치면, 종료는 **게임이 C 런타임 `exit(0)`(또는 `WinMain` 반환)으로 정상 종료**하는 것이고, CRT teardown이 `.protect`의 ExitProcess thunk로 들어갑니다. 종료 코드 0, crash 아님.

#### 결론

ez2d2m는 보호를 통과해 실제 게임을 실행하며, 게임은 캐비닛 출력을 초기화한 뒤 자산을 열기 전에 **`WinMain`에서 0을 반환해 정상 종료**합니다. 입력 대기도, 보호 실패 종료도, crash도 아닙니다.

#### 미확정 — 두 가설로 좁혀짐

- **(a) 게임이 `0x0011` 결과를 검사해 불일치 시 `WinMain`에서 0을 반환한다.** [거부 실험](#2026-09-11-api_code-응답을-넣어도-진행하지-않음--answering-api_code-does-not-advance)은 게스트가 `0x0011` 성공 여부를 검사함을 보였습니다. 다만 echo와 HL_CODE(blocks 0·1·2·4·5) 내용이 같은 결과를 냈으므로, 남은 미검증 부분은 블록 3의 add뿐입니다. 이 가설을 확정·반증하려면 블록 3 add가 필요합니다.
- **(b) 종료가 Hardlock과 무관한 게임 로직이다.** 캐비닛 출력 뒤의 버전·설정·서비스 조건 등. 내용을 "더 맞게"(HL_CODE) 줘도 변화가 없었다는 점이 이쪽을 지지합니다.
- 두 가설을 가르는 결정적 실험은 블록 3 add를 구현해 `0x0011`에 완전한 응답을 주는 것입니다. 그래도 동일하게 종료하면 (a)가 반증되고 원인은 (b)로 확정됩니다.

### English

The exit wrapper was extended to dump, for each `.text` return address on the stack, the code window before it (code addresses and bytes only, no secrets), and the trace tail before the exit was read alongside it.

**Confirmed — the sequence before the exit.** A run's last actions are: reopen the device, two handshakes and a Function `0x0000` descriptor; read `EZ2Dancer.ini` (712 bytes) from the CHD; the seven-block `function=0x0011` transform; **eight 16-bit writes** to port `0x030a` (`0x0004, 0x0104, 0x0304, 0x0704, 0x0f04, 0x0f05, 0x0f07, 0x0f07`); two Function `0x0001` (API_DOWN) descriptors tearing down the dongle session; then `ExitProcess(0)`.

**Confirmed — no input port is ever read.** `io-port:dir=read` is zero: the guest only drives cabinet **output** and never reads an input, so the exit is a deterministic early termination, not a wait on input.

**Confirmed — a CRT shutdown path.** The dynamic-resolver names just before the exit are `GetACP`, `GetOEMCP`, `GetStringTypeW`, `GetStringTypeA`, `GetTimeZoneInformation` and `HeapDestroy` — MSVCRT locale and shutdown functions. With the game `.text` frames from the C2 section, the exit is the **game terminating normally through the C runtime `exit(0)` (or a `WinMain` return)**, with the CRT teardown running into the `.protect` ExitProcess thunk. Exit code 0, not a crash.

**Conclusion.** ez2d2m passes the protection and runs the actual game, which initializes cabinet output and then **returns 0 from `WinMain`, terminating normally before opening any asset** — not a wait on input, not a protection-failure exit, and not a crash.

**Unresolved — narrowed to two hypotheses.** (a) The game inspects the `0x0011` result and returns 0 from `WinMain` on a mismatch: the [rejection experiment](#2026-09-11-api_code-응답을-넣어도-진행하지-않음--answering-api_code-does-not-advance) showed the guest checks whether `0x0011` succeeds, and only block 3's add is left unverified, so confirming or refuting this needs the block-3 add. (b) The exit is game logic unrelated to Hardlock — a version, config or service condition after cabinet output; that making the content more nearly correct (`HL_CODE`) changed nothing supports this. The decisive experiment separating them is to implement the block-3 add and give `0x0011` a complete answer: if it still exits identically, (a) is refuted and the cause is (b).

---

## 2026-09-11 `API_CODE` 7블록 payload의 어느 부분이 쓰이는가 / Which parts of the API_CODE payload matter

### 한국어

작업 247이 남긴 질문은 "게스트가 `0x0011` 응답의 내용을 검사하는가"였습니다. 그때까지 블록 3(더하기)과 블록 6은 변화시켜 본 적이 없었습니다. 요청 행으로 블록을 하나씩 바꿔 실행해 확인했습니다.

#### 이 머신의 7블록 입력 — 확인됨

두 번 받아 대조했습니다. 블록 번호는 0부터입니다.

| 블록 | 실행 간 |
| --- | --- |
| 0, 1, 3, 4, 5, 6 | 같음 |
| 2 | **다름** (주소로 보이는 dword를 담음) |

블록 5 `9e7a5bdc508d3873`은 다른 머신 관찰과도 같습니다. 작업 247이 "계산 입력은 변하지 않는 블록 5"라고 본 것과 일치합니다.

#### 시험 — 확인됨

요청 행의 입력 패턴은 블록 3·5·6만 지정하고 나머지는 `??`로 두었습니다. 출력만 바꾼 세 map을 각각 두 번 실행했습니다.

| map | 출력에서 바꾼 것 | Hardlock 요청 | io-port | 종료 |
| --- | --- | --- | --- | --- |
| A (대조군) | 블록 5를 같은 값으로 되씀 | total 32, descriptor 15, transform 12 | 8 | `0` |
| B3 | 블록 3의 마지막 바이트 +1 | **A와 동일** | 8 | `0` |
| B6b | 블록 6의 두 번째 dword 변경 | total 31, descriptor 14 | **0** | **`0xC0000005`** |
| B6a | 블록 6의 **첫** dword 변경 | A와 동일 | 8 | `0` |

두 실행이 각각 같은 값을 냈습니다.

#### 확인됨 — 블록 6의 두 번째 dword는 게스트 주소입니다

블록 6은 `001d6b5f00db1900`이고 little-endian dword 둘로 읽으면 `0x5f6b1d00`과 `0x0019db00`입니다. 앞은 injected runtime 적재 대역, 뒤는 게스트 스택 주소 대역입니다.

**두 번째 dword를 바꾸면 게스트가 치명적 access violation으로 죽습니다.** 램프 순서열도 실행하지 못하고(`io-port` 0건) descriptor도 하나 적습니다. 첫 dword는 바꿔도 대조군과 구분되지 않습니다.

따라서 블록 6은 응답의 일부가 아니라 **게스트가 넘겨 준 포인터**이며, 응답 행은 이 블록을 그대로 되돌려 주어야 합니다. 2EZConfig-V2 계약 요약이 "마지막 블록은 유지한다"고 한 것과 맞습니다.

#### 미확정 — 블록 0–5의 검사 여부는 아직 판정되지 않았습니다

B3가 대조군과 같다는 사실은 **"게스트가 블록 3을 무시한다"를 뜻하지 않습니다.** echo와 echo+1은 둘 다 틀린 답일 수 있고, 틀린 답끼리 구분되지 않는 것은 당연합니다. 작업 247이 echo와 `HL_CODE` 답이 블록 0·1·2·4·5에서 달라도 downstream이 같다고 기록한 것도 같은 한계를 갖습니다.

**두 틀린 답 사이에 관측 가능한 차이가 없다**가 지금까지 말할 수 있는 전부입니다. 옳은 답만이 차이를 낼 수 있습니다.

#### 추정 — 답이 payload가 아니라 포인터가 가리키는 곳으로 갈 가능성

블록 6의 두 번째 dword가 게스트 스택을 가리키고, payload에 `cc` 채움(초기화되지 않은 스택)이 섞여 있다는 점을 함께 보면, 이 7블록은 게스트가 스택에 잡은 구조체이고 그 안의 포인터가 **동글이 답을 쓸 버퍼**일 수 있습니다. descriptor 요청의 `data_address`와 같은 모양입니다.

이것이 맞다면 payload 블록을 어떻게 채워도 게스트가 보는 값은 바뀌지 않으므로, 블록 0–5 변경이 관측 차이를 내지 않는 것도 설명됩니다. re2DJ에는 게스트 메모리에 쓰는 경로가 없습니다.

**추정입니다.** 포인터를 망가뜨리면 죽는다는 것은 게스트가 그 주소를 쓴다는 뜻일 뿐, 읽는지 쓰는지는 구분하지 않았습니다.

### English

Task 247 left the question of whether the guest inspects the content of the `0x0011` answer; block 3 (the add) and block 6 had never been varied. Request rows changing one block at a time settle part of it.

#### This machine's seven-block input — confirmed

Captured twice: blocks 0, 1, 3, 4, 5 and 6 are identical between runs and only block 2 differs, holding what looks like an address. Block 5 `9e7a5bdc508d3873` also matches the other machine's observation, agreeing with task 247's reading that the computation input is the block that does not move.

#### The test — confirmed

The request row's input pattern specified only blocks 3, 5 and 6, leaving the rest `??`; three maps differing only in output were each run twice, with identical figures per map.

A control that writes block 5 back unchanged gives 32 Hardlock requests, 15 descriptors, 12 transforms, eight port writes and exit code `0`. Changing block 3's last byte gives **exactly the same**. Changing block 6's **second** dword gives 31 requests, 14 descriptors, **no port writes at all** and a fatal **`0xC0000005`**. Changing block 6's **first** dword is indistinguishable from the control.

#### Confirmed — block 6's second dword is a guest address

Block 6 is `001d6b5f00db1900`, two little-endian dwords `0x5f6b1d00` and `0x0019db00` — the first in the injected runtime's load region, the second in the guest stack range.

**Corrupting the second dword kills the guest with an access violation** before it even runs the lamp sequence. Corrupting the first is indistinguishable from the control.

Block 6 is therefore not part of the answer but **a pointer the guest supplied**, and a response row has to return it unchanged — which agrees with the 2EZConfig-V2 contract summary's "the last block is left unchanged".

#### Unresolved — whether blocks 0 to 5 are checked is still undecided

That B3 matches the control does **not** mean the guest ignores block 3. Echo and echo-plus-one may both be wrong answers, and two wrong answers being indistinguishable proves nothing. Task 247's finding that echo and the `HL_CODE` answer differ across blocks 0, 1, 2, 4 and 5 with identical downstream carries the same limit.

**No observable difference between two wrong answers** is all that can be said so far; only a correct answer could produce one.

#### Inferred — the answer may go to the pointer's target rather than the payload

Taken with the `cc` filler in the payload, which is uninitialised stack, the second dword pointing into the guest stack suggests these seven blocks are a stack structure whose pointer names **the buffer the dongle writes its answer into** — the same shape as a descriptor request's `data_address`.

If that holds, nothing written into the payload blocks changes what the guest reads, which would also explain why varying blocks 0 to 5 produces no observable difference. re2DJ has no path that writes guest memory.

**This is inferred.** That corrupting the pointer is fatal shows only that the guest uses the address; whether it reads or writes there was not distinguished.
