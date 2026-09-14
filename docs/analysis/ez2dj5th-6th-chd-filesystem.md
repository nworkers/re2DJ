# ez2dj5th·ez2dj6th CHD 파일시스템 관찰

## 한국어

### 범위와 상태

이 문서는 사용자가 제공한 `roms/ez2dj5th/ez2dj5.chd`와 `roms/ez2dj6th/6th.chd`를 현재 CHD/FAT32 probe로 읽은 결과를 기록합니다. 원본 이미지와 실행 파일은 저장소에 추가하지 않습니다.

### 확인됨

- 5th `ez2dj5.chd`는 CHD v5, logical bytes `20,842,827,264`, hunk bytes `4096`, unit bytes `512`입니다. MBR signature는 `55aa`입니다.
- 현재 `Fat32Volume`은 5th 이미지에서 in-range FAT32 partition을 찾지 못해 `CHD MBR has no in-range FAT32 partition`으로 종료합니다.
- 6th `6th.chd`는 CHD v5, logical bytes `20,847,697,920`, hunk bytes `4096`, unit bytes `512`입니다. MBR signature는 `55aa`이며 partition 0에서 FAT32 volume을 확인했습니다.
- 6th의 `EZ2DJ` 디렉터리에는 `EZ2DJ.EXE`, `EZ2DJ6th.EXE`, `EZ2DJ.INI`, `FONTKR.DAT`, `FONTEN.DAT`, `BG`, `SOUND`, `SYSTEM`이 있습니다.
- 6th `EZ2DJ/EZ2DJ.EXE`는 PE32/i386, image base `0x00400000`, entry RVA `0x000153ff`, sections 3인 bootstrap입니다. import와 문자열에 `CreateProcessA` 및 `.\\EZ2DJ6TH.EXE`가 확인되었습니다.
- 6th `EZ2DJ/EZ2DJ6th.EXE`는 PE32/i386, image base `0x00400000`, entry RVA `0x000667d4`, SizeOfImage `0x00e34000`이며 DirectDraw, DirectSound, DirectInput을 import하는 실제 게임 실행파일입니다.
- 6th `FONTKR.DAT`는 directory entry 크기 75,200 bytes에 비해 FAT chain이 짧아 현재 전체 읽기에 실패합니다. BPB `ExtFlags=0x0000`이고 두 FAT 복사본의 관련 항목이 같으므로 active FAT 선택 문제는 아닙니다.

### 2026-09-15 추가 확인

- `roms/ez2dj5th/ez2dj/`와 `roms/ez2dj6th/extracted/`에 디렉터리 배치가 staged되어 있습니다. 5th의 `EZ2DJ.exe`는 1,388,544바이트, PE TimeDateStamp `0x3f53377b`(2003-09-01)이고 진입점이 `.protect`에 있는 보호된 빌드입니다. 5th의 내부 실행 파일 이름과 구조는 이로써 확인되었으나, 이 디렉터리가 `ez2dj5.chd`에서 나온 것인지는 여전히 **미확정**입니다. 5th 이미지의 파일시스템을 열지 못해 해시 비교를 할 수 없기 때문입니다.
- `roms/ez2dj6th/extracted/EZ2DJ/`의 `EZ2DJ.EXE`와 `EZ2DJ6th.EXE`는 `6th.chd` 안의 같은 이름 파일과 MD5·SHA-1·SHA-256이 모두 같습니다. 6th는 입력에 따라 빌드가 갈리지 않습니다.
- 6th bootstrap의 평문 문자열에는 자식 경로가 **둘** 있습니다. 위에 기록한 `.\EZ2DJ6TH.EXE` 외에 `.\EZ2DJ1ST\EZ2DJ.EXE`가 raw offset `0x1b094`에 있고, `%s\EZ2DJ1ST`가 `0x1b088`에 있습니다. 어떤 조건에서 어느 자식을 고르는지는 **미확정**입니다.
- `6th.chd`의 `EZ2DJ/Ez2Dj1st/`는 완전한 1st Tracks 배치입니다. 그 `Ez2DJ.exe`는 360,448바이트, PE TimeDateStamp `0x411bbf5c`(2004-08-12)이고 **보호 섹션이 없습니다.** 진입점 RVA `0x00036f30`은 `.text`에 있고 섹션은 셋뿐이며 import는 6 DLL / 138 함수입니다. 1999년 보호 빌드와 같은 게임의 보호되지 않은 2004년 재빌드입니다.
- 이 배치의 `Test.exe`, `PlzPowerOff.exe`, `AllowIo.exe`, `PortTalk.sys`는 `roms/ez2dj1st/ez2dj1/`의 같은 이름 파일과 SHA-256이 같습니다. `Ez2DJ.exe`만 다릅니다.
- 6th bootstrap과 게임 본체, 동봉 1st Tracks 빌드 셋 다 `HARDLOCK.VXD`·`FEnteDev` 장치 문자열을 평문으로 담습니다. Hardlock 경계는 자식 프로세스만의 것이 아닙니다.
- 6th의 `EZ2DJ/EZ2DJ.INI`(816바이트)는 평문 INI가 아닙니다. 같은 디렉터리의 `bookkeeping.ini`(162바이트)는 평문입니다. `EZ2DJ6th.EXE`가 `GetPrivateProfileIntA`와 `WritePrivateProfileStringA`를 import하므로 평문 경로가 있으며, `EZ2DJ.INI`가 실행 중 복호화되는지는 **미확정**입니다.

구조 상세는 [실행 파일 구조 분석](ez2dj-exe-structures.md) 7절과 8절에 있습니다.

### 추정 및 미확정

- **확인됨:** 6th 프로파일은 `EZ2DJ/EZ2DJ.EXE` bootstrap을 선택하고, `DEBUG_PROCESS`로 생성된 `EZ2DJ6th.EXE` child에 HLE를 주입해야 합니다. 실제 child를 직접 실행하면 launcher 계약 오류가 발생합니다.
- **확인됨:** 명시적 synthetic baseline과 candidate map을 사용한 진단에서 `child_process_created`, child runtime 준비, child 종료 경계까지 확인했습니다. 이는 process-follow plumbing의 확인이며, 6th의 유효한 Hardlock 응답이나 게임 실행 성공을 의미하지 않습니다.
- **확인됨:** 6th 실제 실행파일은 읽기 전용 `.rdata`에 IAT가 있어 HLE slot patch에 임시 쓰기 권한이 필요합니다.
- **확인됨:** 4th Hardlock material을 값 비공개 진단으로 적용하면 6th가 handshake 2회와 descriptor 2회를 수행하지만, 최종적으로 주 진입 함수가 `-1`을 반환해 CRT 종료부 `0x00466cc5`에서 종료합니다. 이는 6th Hardlock 계약의 호환성을 확정하지 않습니다.
- **확인됨:** 6th의 두 descriptor 요청은 `function=0x0000`과 `function=0x0001`이고 공통 `module_address=0x4c51`, `remote=0x0001`, `port=0x0378`을 가집니다. `id_ref`와 `id_verify`는 두 요청에서 동일한 값이지만 원문은 저장소 문서에 기록하지 않고 사용자 로컬 `cfg/hardlock-id.ini`에만 남겼습니다.
- **확인됨:** launcher의 `--hardlock-descriptor-dump <path>` 옵션은 첫 번째 유효한 256바이트 descriptor에서 `module_address`, `id_ref`, `id_verify`를 지정한 Git-ignored 로컬 파일에 기록하며, 일반 VFS trace에는 ID 해시만 남깁니다. 이 기능은 이후 프로파일 추가 때 재사용할 수 있습니다.
- **미확정:** 5th 이미지의 실제 파티션 종류와 파일시스템. 현재 결과만으로 FAT16이라고 확정하지 않습니다. 5th의 내부 실행 파일 이름과 구조는 위 2026-09-15 항목에서 디렉터리 배치로 확인했으나, 그 배치가 이 이미지에서 나왔는지는 여전히 미확정입니다.
- **미확정:** 5th·6th의 Hardlock 응답, 6th raw-I/O helper RVA와 장치 계약 및 최종 원본 실행 성공. 4th raw-I/O RVA는 6th 프로파일에서 비활성화합니다.

따라서 5th·6th 프로파일은 4th 호환성 기준으로 등록하되, 이 문서의 관찰은 프로파일 등록과 독립적인 CHD 실행 성공을 의미하지 않습니다.

## English

### Scope and status

This document records the current CHD/FAT32 probe results for the user-provided `roms/ez2dj5th/ez2dj5.chd` and `roms/ez2dj6th/6th.chd`. Original images and executables are not added to the repository.

### Confirmed

- The 5th `ez2dj5.chd` is CHD v5 with `20,842,827,264` logical bytes, 4096-byte hunks, and 512-byte units. Its MBR signature is `55aa`.
- The current `Fat32Volume` rejects the 5th image with `CHD MBR has no in-range FAT32 partition`.
- The 6th `6th.chd` is CHD v5 with `20,847,697,920` logical bytes, 4096-byte hunks, and 512-byte units. Its MBR signature is `55aa`, and partition 0 is recognized as a FAT32 volume.
- The 6th `EZ2DJ` directory contains `EZ2DJ.EXE`, `EZ2DJ6th.EXE`, `EZ2DJ.INI`, `FONTKR.DAT`, `FONTEN.DAT`, `BG`, `SOUND`, and `SYSTEM`.
- The 6th `EZ2DJ/EZ2DJ.EXE` is a PE32/i386 bootstrap with image base `0x00400000`, entry RVA `0x000153ff`, and three sections. Its imports and strings contain `CreateProcessA` and `.\\EZ2DJ6TH.EXE`.
- The 6th `EZ2DJ/EZ2DJ6th.EXE` is the actual PE32/i386 game executable, with image base `0x00400000`, entry RVA `0x000667d4`, SizeOfImage `0x00e34000`, and DirectDraw, DirectSound, and DirectInput imports.
- The 6th `FONTKR.DAT` directory entry declares 75,200 bytes, but its FAT chain is shorter and a complete read currently fails. BPB `ExtFlags=0x0000` and matching relevant entries in both FAT copies rule out active-FAT selection.

### Additions confirmed 2026-09-15

- Directory layouts are staged under `roms/ez2dj5th/ez2dj/` and `roms/ez2dj6th/extracted/`. The 5th `EZ2DJ.exe` is 1,388,544 bytes with PE TimeDateStamp `0x3f53377b` (2003-09-01) and is a protected build with its entry point in `.protect`. The 5th internal executable's name and structure are thereby confirmed, but whether this directory came from `ez2dj5.chd` remains **unresolved**, since the 5th image's filesystem cannot be opened for a hash comparison.
- `EZ2DJ.EXE` and `EZ2DJ6th.EXE` under `roms/ez2dj6th/extracted/EZ2DJ/` match their counterparts inside `6th.chd` on MD5, SHA-1 and SHA-256. 6th does not split into different builds by input.
- The 6th bootstrap's plaintext strings hold **two** child paths: besides the `.\EZ2DJ6TH.EXE` recorded above, `.\EZ2DJ1ST\EZ2DJ.EXE` sits at raw offset `0x1b094` and `%s\EZ2DJ1ST` at `0x1b088`. Which condition selects which child is **unresolved**.
- `EZ2DJ/Ez2Dj1st/` inside `6th.chd` is a complete 1st Tracks layout. Its `Ez2DJ.exe` is 360,448 bytes with PE TimeDateStamp `0x411bbf5c` (2004-08-12) and **carries no protection section**: entry RVA `0x00036f30` in `.text`, three sections only, and imports of 6 DLLs / 138 functions. It is an unprotected 2004 rebuild of the same game as the 1999 protected build.
- That layout's `Test.exe`, `PlzPowerOff.exe`, `AllowIo.exe` and `PortTalk.sys` share their SHA-256 with the same-named files under `roms/ez2dj1st/ez2dj1/`; only `Ez2DJ.exe` differs.
- All three — the 6th bootstrap, the game body, and the bundled 1st Tracks build — carry the `HARDLOCK.VXD` and `FEnteDev` device strings in plaintext. The Hardlock boundary is not the child process's alone.
- The 6th `EZ2DJ/EZ2DJ.INI` (816 bytes) is not plaintext INI, while `bookkeeping.ini` (162 bytes) in the same directory is. `EZ2DJ6th.EXE` imports `GetPrivateProfileIntA` and `WritePrivateProfileStringA`, so a plaintext path exists; whether `EZ2DJ.INI` is decrypted at runtime is **unresolved**.

Structural detail is in sections 7 and 8 of the [executable structures analysis](ez2dj-exe-structures.md).

### Inferred and unresolved

- **Confirmed:** the 6th profile selects the `EZ2DJ/EZ2DJ.EXE` bootstrap and injects HLE into its `EZ2DJ6th.EXE` child observed through `DEBUG_PROCESS`. Directly launching the actual child produces the launcher-contract error.
- **Confirmed:** an explicit synthetic baseline and candidate map reached `child_process_created`, child runtime preparation, and the child exit boundary. This confirms process-follow plumbing only; it does not confirm a valid 6th Hardlock response or successful game execution.
- **Confirmed:** the actual 6th executable places its IAT in read-only `.rdata`, requiring temporary write access for HLE slot patching.
- **Confirmed:** a value-redacted diagnostic using the 4th Hardlock material makes 6th issue two handshakes and two descriptor requests, but its main entry still returns `-1` and exits through the CRT at `0x00466cc5`. This does not confirm compatibility of the 6th Hardlock contract.
- **Confirmed:** the two 6th descriptor requests use `function=0x0000` and `function=0x0001`, with common `module_address=0x4c51`, `remote=0x0001`, and `port=0x0378`. `id_ref` and `id_verify` are identical across both requests, but their raw values are kept only in the user's local `cfg/hardlock-id.ini`, not in repository analysis.
- **Confirmed:** the launcher option `--hardlock-descriptor-dump <path>` writes `module_address`, `id_ref`, and `id_verify` from the first valid 256-byte descriptor to the specified Git-ignored local file, while normal VFS traces retain only ID digests. This is reusable for future profile creation.
- **Unresolved:** the 5th image's partition type and filesystem. The current result does not justify calling it FAT16. The 5th internal executable's name and structure are confirmed from the directory layout in the 2026-09-15 additions above, but whether that layout came from this image is still unresolved.
- **Unresolved:** Hardlock responses for 5th/6th, 6th raw-I/O helper RVAs and device contract, and final original execution success. The 4th raw-I/O RVAs are disabled for the 6th profile.

The 5th/6th profiles therefore use the 4th compatibility baseline, but these observations do not constitute independent successful CHD execution for either profile.
