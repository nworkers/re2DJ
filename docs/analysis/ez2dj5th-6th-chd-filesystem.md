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

### 추정 및 미확정

- **확인됨:** 6th 프로파일은 `EZ2DJ/EZ2DJ.EXE` bootstrap을 선택하고, `DEBUG_PROCESS`로 생성된 `EZ2DJ6th.EXE` child에 HLE를 주입해야 합니다. 실제 child를 직접 실행하면 launcher 계약 오류가 발생합니다.
- **확인됨:** 명시적 synthetic baseline과 candidate map을 사용한 진단에서 `child_process_created`, child runtime 준비, child 종료 경계까지 확인했습니다. 이는 process-follow plumbing의 확인이며, 6th의 유효한 Hardlock 응답이나 게임 실행 성공을 의미하지 않습니다.
- **확인됨:** 6th 실제 실행파일은 읽기 전용 `.rdata`에 IAT가 있어 HLE slot patch에 임시 쓰기 권한이 필요합니다.
- **확인됨:** 4th Hardlock material을 값 비공개 진단으로 적용하면 6th가 handshake 2회와 descriptor 2회를 수행하지만, 최종적으로 주 진입 함수가 `-1`을 반환해 CRT 종료부 `0x00466cc5`에서 종료합니다. 이는 6th Hardlock 계약의 호환성을 확정하지 않습니다.
- **확인됨:** 6th의 두 descriptor 요청은 `function=0x0000`과 `function=0x0001`이고 공통 `module_address=0x4c51`, `remote=0x0001`, `port=0x0378`을 가집니다. `id_ref`와 `id_verify`는 두 요청에서 동일한 값이지만 원문은 저장소 문서에 기록하지 않고 사용자 로컬 `cfg/hardlock-id.ini`에만 남겼습니다.
- **확인됨:** launcher의 `--hardlock-descriptor-dump <path>` 옵션은 첫 번째 유효한 256바이트 descriptor에서 `module_address`, `id_ref`, `id_verify`를 지정한 Git-ignored 로컬 파일에 기록하며, 일반 VFS trace에는 ID 해시만 남깁니다. 이 기능은 이후 프로파일 추가 때 재사용할 수 있습니다.
- **미확정:** 5th의 실제 파티션 종류, 파일시스템, 내부 실행 파일 경로. 현재 결과만으로 FAT16이라고 확정하지 않습니다.
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

### Inferred and unresolved

- **Confirmed:** the 6th profile selects the `EZ2DJ/EZ2DJ.EXE` bootstrap and injects HLE into its `EZ2DJ6th.EXE` child observed through `DEBUG_PROCESS`. Directly launching the actual child produces the launcher-contract error.
- **Confirmed:** an explicit synthetic baseline and candidate map reached `child_process_created`, child runtime preparation, and the child exit boundary. This confirms process-follow plumbing only; it does not confirm a valid 6th Hardlock response or successful game execution.
- **Confirmed:** the actual 6th executable places its IAT in read-only `.rdata`, requiring temporary write access for HLE slot patching.
- **Confirmed:** a value-redacted diagnostic using the 4th Hardlock material makes 6th issue two handshakes and two descriptor requests, but its main entry still returns `-1` and exits through the CRT at `0x00466cc5`. This does not confirm compatibility of the 6th Hardlock contract.
- **Confirmed:** the two 6th descriptor requests use `function=0x0000` and `function=0x0001`, with common `module_address=0x4c51`, `remote=0x0001`, and `port=0x0378`. `id_ref` and `id_verify` are identical across both requests, but their raw values are kept only in the user's local `cfg/hardlock-id.ini`, not in repository analysis.
- **Confirmed:** the launcher option `--hardlock-descriptor-dump <path>` writes `module_address`, `id_ref`, and `id_verify` from the first valid 256-byte descriptor to the specified Git-ignored local file, while normal VFS traces retain only ID digests. This is reusable for future profile creation.
- **Unresolved:** the 5th partition type, filesystem, and internal executable path. The current result does not justify calling it FAT16.
- **Unresolved:** Hardlock responses for 5th/6th, 6th raw-I/O helper RVAs and device contract, and final original execution success. The 4th raw-I/O RVAs are disabled for the 6th profile.

The 5th/6th profiles therefore use the 4th compatibility baseline, but these observations do not constitute independent successful CHD execution for either profile.
