# 작업 로그: ez2dj1stse CHD 프로파일 전환

## 한국어

### 관련 문서

- 설계: [ez2dj1stse CHD 프로파일 전환 설계](../design/20260908-222-ez2dj1stse-chd-profile.md)
- 작업 지시: [ez2dj1stse CHD 프로파일 전환](../work-orders/20260908-222-ez2dj1stse-chd-profile.md)
- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)

### 수행 내용

1. `re2dj_chd_probe`로 `roms/ez2dj1stse/ez2dj1stse.chd`의 CHD header, GDDD metadata, FAT32 구조, 루트/`ez2dj` 디렉터리 목록, 내부 실행 파일 PE 정보를 읽었습니다.
2. CHD 내부 `ez2dj/Ez2DJ.exe`를 임시 경로에 덤프해 기존 추출본 `roms/ez2dj1stse/ez2dj/ez2dj.exe`와 섹션 단위로 비교했습니다.
3. CHD의 `WINDOWS/SYSTEM.INI`, `WINDOWS/WIN.INI`, StartUp 바로 가기, `ez2dj/SYSTEM.INI`를 읽어 게스트 부팅 경로를 확인했습니다.
4. `src/target/target_profile.cpp`의 `ez2dj1stse`를 MAME CHD profile로 전환했습니다.
5. `tests/unit/target_profile_test.cpp`의 1st SE 블록을 CHD shortcut 검증으로 다시 쓰고, 1st SE 레이아웃에 의존하던 nested-root와 incomplete-dump 블록을 디렉터리 프로파일인 2nd 레이아웃으로 옮겼습니다.
6. `ARCHITECTURE.md`, `README.md`, `docs/analysis/README.md`를 갱신했습니다.

### 코드 변경

`ez2dj1stse` built-in profile:

- `hdd_input_kind`를 `kMameChd`로 설정
- `default_hdd_image_relative_path`를 `roms/ez2dj1stse`로 추가
- `executable_relative_path`를 `ez2dj/Ez2DJ.exe`로 설정
- `guest_drive_letter`를 `D`에서 `C`로 정정
- profile note를 `.protect` 보호 계열과 호환성 기준선 성격을 밝히는 내용으로 교체
- `default_hdd_directory_relative_path`, HLE/LPTDI/legacy I/O/demo volume/detached 기본값, fingerprint는 유지

### 확인된 사실

CHD 구조는 CHD v5, logical bytes `4,310,433,792`, hunk `4096`, FAT32 partition LBA `63`, data LBA `16505`, cluster count `1,050,194`, volume label `EZ2DJ_SE`입니다. 내부 실행 파일은 `ez2dj/Ez2DJ.exe`, size `634,880`, entry RVA `0x01ad1240`, sections `6`입니다.

게스트 부팅 경로는 `WINDOWS/Start Menu/Programs/StartUp/Ez2DJ.exe.lnk`가 가리키는 `C:\ez2dj\Ez2DJ.exe`입니다. `WINDOWS/SYSTEM.INI`의 `shell=`은 `Explorer.exe`이고 `WIN.INI`의 `run=`·`load=`는 비어 있습니다.

CHD 실행 파일은 추출본과 다른 파일입니다. CHD는 `.protect` RWX 래퍼(6 sections, `.text` 엔트로피 `7.99`), 추출본은 `.gtide`/`.gdata`/`.gidata` 래퍼(8 sections, `.text` 엔트로피 `6.21`)입니다. 두 파일은 PE timestamp `0x3862df27`이 같고 앞 5개 섹션 배치가 같으며 `.idata` raw 바이트가 완전히 동일합니다. 추출본 `.text`의 RVA `0x00038987`에 `ec`, `0x000389ab`에 `ee`가 실제로 있는 것도 확인했습니다.

### 검증

- `cmake --build build/windows-x86 --config Release --target re2dj re2dj_unit_tests re2dj_windows_product_loader_probe` 성공
- `re2dj_unit_tests.exe` → `checks: 1418, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → `profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`
- `re2dj ez2dj1stse --list-targets` → CHD image `roms/ez2dj1stse/ez2dj1stse.chd`, FAT32 `label=EZ2DJ_SE data_lba=16505 clusters=1050194`, target `ez2dj1stse ez2dj/Ez2DJ.exe built-in`
- `re2dj ez2dj1stse` → entry section `.protect`, entry RVA `0x01ad1240`, sections `6`

`build/windows-x86` 전체 Release 빌드는 `re2dj_windows_injected_runtime.dll`이 실행 중인 EZ2DJ 프로세스 두 개에 잠겨 있어 링크되지 않았습니다. 이 작업의 변경 범위(`target_profile.cpp`, 테스트, 문서)는 위 세 타깃에서 모두 검증되었고, injected runtime은 이 변경으로 동작이 달라지지 않습니다.

### CHD 빌드 첫 실행 관측

`re2dj ez2dj1stse`는 positional 형태만으로 실행까지 진행하는 명령이라, 검증 중 실제 실행이 한 번 발생했습니다. 진단 로그는 `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-004923-080.jsonl`입니다.

CHD staging, VFS mount, LPTDI target state, io-port runtime, display/DirectSound/DirectInput 준비는 모두 성립했고 entry breakpoint도 `0x01ed1240`에서 hit했습니다. 그러나 `handoff_prepared=false`, `iat_verified=false`로 `runtime handoff preparation failed`에서 멈췄습니다. 남은 프로세스는 없습니다.

이는 이 전환의 예상된 결과입니다. 지금까지 확인된 1st SE 실행 계약은 `.gtide` 빌드에서 얻은 것이고, CHD의 `.protect` 빌드는 import directory와 IAT를 보호 섹션이 소유합니다.

### 남은 과제

- `.protect` 복호화 이후 시점에서 import directory를 다시 읽어 IAT 기반 handoff를 성립시키는 경로 조사
- legacy I/O helper RVA `0x00038987` / `0x000389ab`가 `.protect` 빌드에서도 유효한지 실행으로 확인
- `.protect` 빌드의 Hardlock 장치 이름·IOCTL 시퀀스·유효 응답 확인
- 추출 `.gtide` 덤프에 대한 built-in 정책이 필요해지면 별도 디렉터리 프로파일 추가 여부 재검토

## English

### Related documents

- Design: [ez2dj1stse CHD Profile Conversion Design](../design/20260908-222-ez2dj1stse-chd-profile.md)
- Work order: [ez2dj1stse CHD Profile Conversion](../work-orders/20260908-222-ez2dj1stse-chd-profile.md)
- Analysis: [ez2dj1stse CHD Filesystem Analysis](../analysis/ez2dj1stse-chd-filesystem.md)

### What was done

`re2dj_chd_probe` read the CHD header, GDDD metadata, FAT32 structure, root and `ez2dj` directory listings, and the internal executable's PE information from `roms/ez2dj1stse/ez2dj1stse.chd`. The internal `ez2dj/Ez2DJ.exe` was dumped to a temporary path and compared section by section against the existing extracted `roms/ez2dj1stse/ez2dj/ez2dj.exe`. The CHD's `WINDOWS/SYSTEM.INI`, `WINDOWS/WIN.INI`, StartUp shortcut, and `ez2dj/SYSTEM.INI` were read to establish the guest boot path.

`ez2dj1stse` in `src/target/target_profile.cpp` was converted to an MAME CHD profile. The 1st SE block of `tests/unit/target_profile_test.cpp` was rewritten as a CHD-shortcut verification, and the nested-root and incomplete-dump blocks that depended on the 1st SE layout were moved onto the 2nd layout, which is still a directory profile. `ARCHITECTURE.md`, `README.md`, and `docs/analysis/README.md` were updated.

### Code change

The `ez2dj1stse` built-in profile now sets `hdd_input_kind` to `kMameChd`, adds `default_hdd_image_relative_path` `roms/ez2dj1stse`, sets `executable_relative_path` to `ez2dj/Ez2DJ.exe`, corrects `guest_drive_letter` from `D` to `C`, and replaces the note with one that states the `.protect` protection family and the compatibility-baseline nature of the defaults. `default_hdd_directory_relative_path`, the HLE/LPTDI/legacy-I/O/demo-volume/detached defaults, and the fingerprint are unchanged.

### Confirmed facts

The CHD is v5 with 4,310,433,792 logical bytes and 4,096-byte hunks, holding a FAT32 partition at LBA 63 with data LBA 16,505, 1,050,194 clusters, and volume label `EZ2DJ_SE`. Its internal executable is `ez2dj/Ez2DJ.exe` at 634,880 bytes with entry RVA `0x01ad1240` and six sections.

The guest boot path is `C:\ez2dj\Ez2DJ.exe`, taken from `WINDOWS/Start Menu/Programs/StartUp/Ez2DJ.exe.lnk`. `WINDOWS/SYSTEM.INI` sets `shell=Explorer.exe`, and `WIN.INI` has empty `run=` and `load=`.

The CHD executable is a different file from the extracted one: a `.protect` RWX wrapper with six sections and `.text` entropy 7.99, versus a `.gtide`/`.gdata`/`.gidata` wrapper with eight sections and `.text` entropy 6.21. Both carry PE timestamp `0x3862df27`, share the placement of their first five sections, and have byte-identical raw `.idata`. The extracted build's `.text` really does hold `ec` at RVA `0x00038987` and `ee` at RVA `0x000389ab`.

### Verification

- `cmake --build build/windows-x86 --config Release --target re2dj re2dj_unit_tests re2dj_windows_product_loader_probe` succeeded.
- `re2dj_unit_tests.exe` reported `checks: 1418, failures: 0`.
- `re2dj_windows_product_loader_probe.exe` reported `profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`.
- `re2dj ez2dj1stse --list-targets` printed the CHD image `roms/ez2dj1stse/ez2dj1stse.chd`, FAT32 `label=EZ2DJ_SE data_lba=16505 clusters=1050194`, and the target `ez2dj1stse ez2dj/Ez2DJ.exe built-in`.
- `re2dj ez2dj1stse` reported entry section `.protect`, entry RVA `0x01ad1240`, and six sections.

A full Release build of `build/windows-x86` did not link `re2dj_windows_injected_runtime.dll` because two running EZ2DJ processes held the file. Everything this task changed — `target_profile.cpp`, the tests, and documentation — was verified through the three targets above, and the injected runtime's behavior is unaffected by this change.

### First run of the CHD build

`re2dj ez2dj1stse` proceeds to execution from the positional form alone, so one real run happened during verification. The diagnostic log is `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-004923-080.jsonl`.

CHD staging, the VFS mount, the LPTDI target state, the io-port runtime, and the display/DirectSound/DirectInput preparation all succeeded, and the entry breakpoint hit `0x01ed1240`. Execution then stopped at `runtime handoff preparation failed` with `handoff_prepared=false` and `iat_verified=false`. No process was left behind.

That is the expected outcome of this conversion: every confirmed 1st SE execution contract came from the `.gtide` build, while the CHD's `.protect` build keeps its import directory and IAT inside the protection section.

### Remaining work

- Investigate a handoff path that re-reads the import directory once `.protect` has decrypted, so the IAT-based handoff can succeed.
- Confirm by execution whether the legacy-I/O helper RVAs `0x00038987` and `0x000389ab` hold for the `.protect` build.
- Confirm the `.protect` build's Hardlock device name, IOCTL sequence, and valid response.
- Revisit whether a separate directory profile for the extracted `.gtide` dump is needed if its built-in policy becomes necessary again.
