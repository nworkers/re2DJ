# ez2dj4th CHD 파일시스템 분석

## 한국어

### 분석 대상과 상태

이 문서는 사용자가 제공한 `roms/ez2dj4th/4thTrax.chd`를 원본 자산 경로에서 읽어 확인한 구조를 기록한다. 원본 CHD, IMG, 압축 파일과 실행 파일은 저장소에 추가하지 않는다.

* **확인됨**: CHD v5 header는 `libchdr`를 통해 logical bytes 20,060,135,424, hunk bytes 4,096, unit bytes 512를 보고한다.
* **확인됨**: GDDD metadata는 `CYLS:38869,HEADS:16,SECS:63,BPS:512`다.
* **확인됨**: LBA 0의 MBR signature는 `55aa`이고 partition 0은 type `0x0c` FAT32 LBA, start LBA 63, length 39,166,407 sectors다.
* **확인됨**: partition boot sector는 512 bytes/sector, 32 sectors/cluster, reserved 32 sectors, FAT 2개, 9,558 sectors/FAT, root cluster 2, FAT32 type string을 선언한다. 계산된 data LBA는 19,211이다.
* **확인됨**: root directory의 `EZ2DJ` directory는 cluster 3이고, `EZ2DJ/EZ2DJ.EXE`는 first cluster 232,139, size 1,372,160 bytes다.
* **확인됨**: executable directory에는 `EZ2DJ.INI`, `FONTKR.DAT`, `FONTEN.DAT`, `BG`, `SOUND`, `SYSTEM`이 함께 있다. built-in `ez2dj4th` fingerprint는 이 이름들을 사용한다.
* **확인됨**: 전체 recursive short-entry 기준으로 directory 368개, file 29,659개, `.EXE` 195개가 관찰됐다. LFN entry도 관찰됐으므로 reader는 short name만 가정하지 않는다.
* **확인됨**: `EZ2DJ.INI`는 640×480, `UseIOCard=1`, `FullScreen=1` 등을 포함한다. 이 값은 CHD read verification에서 관찰한 설정이며 런타임 override 정책과 동일하다고 단정하지 않는다.
* **확인됨**: `WINDOWS/SYSTEM.INI`의 shell은 `Explorer.exe`다.
* **미확정**: cabinet이 실제로 어떤 Win98 boot sequence에서 `EZ2DJ.EXE`를 실행했는지, 그리고 4th executable이 첫 실행에서 요청하는 전체 API/자산 순서는 아직 확인하지 않았다.

### 실행 연결

`Fat32Volume`은 MBR, BPB, FAT chain과 directory entry를 검증하고 CHD logical sector에서 필요한 file range만 읽는다. Windows x86 launcher는 `EZ2DJ.EXE`와 profile fingerprint에 필요한 작은 sibling 파일만 임시 staging한다. injected runtime에는 CHD path를 전달하고, 게스트 `D:\\ez2dj\\...` read request는 CHD 내부 `EZ2DJ/...` 경로를 가리키는 FAT32-backed pseudo handle로 직접 서비스한다. `LoadImageA`처럼 native host path가 필요한 경우에만 요청 파일 하나를 staging/cache에 materialize하며, 쓰기는 기존 overlay 경계를 유지한다.

실제 CHD probe 결과는 `filesystem=fat32`, `filesystem_executable=EZ2DJ/EZ2DJ.EXE`, `machine=i386`, `magic=PE32`, `subsystem=windows-gui`, image base `0x00400000`, entry RVA `0x006e0240`, sections 6으로 확인됐다.

## English

### Scope and status

This document records structure observed by reading the user-supplied `roms/ez2dj4th/4thTrax.chd`. No original CHD, IMG, compressed image, or executable is added to the repository.

* **Confirmed**: libchdr reports CHD v5, 20,060,135,424 logical bytes, 4,096-byte hunks, and 512-byte units.
* **Confirmed**: GDDD metadata is `CYLS:38869,HEADS:16,SECS:63,BPS:512`.
* **Confirmed**: the MBR signature is `55aa`; partition 0 is FAT32 LBA type `0x0c`, starting at LBA 63 for 39,166,407 sectors.
* **Confirmed**: the partition BPB declares 512-byte sectors, 32 sectors/cluster, 32 reserved sectors, two FATs, 9,558 sectors/FAT, root cluster 2, and FAT32. The computed data LBA is 19,211.
* **Confirmed**: the root `EZ2DJ` directory is cluster 3; `EZ2DJ/EZ2DJ.EXE` starts at cluster 232,139 and is 1,372,160 bytes.
* **Confirmed**: the executable directory contains `EZ2DJ.INI`, `FONTKR.DAT`, `FONTEN.DAT`, `BG`, `SOUND`, and `SYSTEM`; the built-in `ez2dj4th` fingerprint uses these names.
* **Confirmed**: a recursive short-entry scan observed 368 directories, 29,659 files, and 195 `.EXE` files. LFN entries are present, so the reader does not assume short names only.
* **Confirmed**: `EZ2DJ.INI` contains 640×480, `UseIOCard=1`, and `FullScreen=1` among other settings. This is an observed image value, not automatically a runtime override policy.
* **Confirmed**: `WINDOWS/SYSTEM.INI` names `Explorer.exe` as its shell.
* **Unresolved**: the cabinet's complete Win98 boot sequence and the complete first-run API/asset request sequence of the 4th executable remain to be observed.

### Execution connection

`Fat32Volume` validates the MBR, BPB, FAT chains, and directory entries, then reads only requested file ranges from CHD logical sectors. The Windows x86 launcher stages the executable and the small sibling files needed by profile matching. The injected runtime receives the CHD path and serves guest `D:\\ez2dj\\...` reads through FAT32-backed pseudo handles mapped to the image's `EZ2DJ/...` directory. APIs requiring a native host path, such as `LoadImageA`, materialize only the requested file into the staging/cache; writes remain in the existing overlay.

The real-CHD probe reports `filesystem=fat32`, `filesystem_executable=EZ2DJ/EZ2DJ.EXE`, machine `i386`, magic `PE32`, Windows GUI subsystem, image base `0x00400000`, entry RVA `0x006e0240`, and six sections.

### Latest Windows launch boundary (2026-09-01)

* **Confirmed**: after the Debug injected-runtime storage-link fix, `build/windows-x86/bin/Debug/re2dj_windows_injected_runtime.dll` is produced and the real `re2dj ez2dj4th --run` command no longer fails with `cannot find bundled injected runtime`.
* **Confirmed**: the parent CLI now passes the selected `EZ2DJ/EZ2DJ.EXE` staging-relative path to the launcher. The launcher resolves the staged PE32, records the CHD VFS mount, and prepares the image-loader hook.
* **Unresolved**: the first runtime handoff still times out before a `CreateFileA` debug message (`runtime handoff was not observed before timeout`). The current evidence therefore confirms CHD read, staging, process creation, DLL injection, and VFS preparation, but not the first protected 4th asset request or game screen.

### 최신 Windows 실행 경계 (2026-09-01)

* **확인됨**: Debug injected runtime 저장소 링크 수정 후 `build/windows-x86/bin/Debug/re2dj_windows_injected_runtime.dll`이 생성되며, 실제 `re2dj ez2dj4th --run`에서 `cannot find bundled injected runtime` 오류가 사라졌습니다.
* **확인됨**: 부모 CLI가 선택한 `EZ2DJ/EZ2DJ.EXE` staging 상대 경로를 launcher에 전달합니다. launcher는 staging PE32를 검증하고 CHD VFS mount와 image-loader hook 준비를 기록했습니다.
* **미확정**: 첫 `CreateFileA` debug message 이전에 `runtime handoff was not observed before timeout`으로 종료됩니다. 따라서 현재 확인 범위는 CHD read, staging, process 생성, DLL 주입, VFS 준비까지이며, 4th 보호 실행의 첫 자산 요청과 게임 화면은 아직 확인되지 않았습니다.
