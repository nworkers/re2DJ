# 작업 로그: ez2dj3rd CHD 프로파일 전환

## 한국어

### 수행 내용

- `ez2dj3rd` 내장 프로파일의 HDD 입력 형식을 추출 디렉터리에서 MAME CHD로 변경했습니다.
- 기본 검색 경로는 `roms/ez2dj3rd`로 유지하고, CHD 내부 대표 실행 파일을 `EZ2DJ/EZ2DJ.EXE`로 지정했습니다.
- 기존 3rd VFS, DirectSound, dynamic resolver, active-console, Hardlock 설정, detached 실행 정책은 변경하지 않았습니다.
- CHD 전환으로 추출 디렉터리 탐색 테스트가 3rd 내장 프로파일을 잘못 선택하지 않도록 단위 테스트를 갱신했습니다.
- 원본 CHD, IMG, 실행 파일은 저장소에 추가하지 않았습니다.

### 확인된 CHD 구조

사용자가 제공한 `roms/ez2dj3rd/ez2dj3rd.chd`를 CHD probe로 읽어 다음을 확인했습니다.

- CHD v5, logical bytes `10,262,568,960`, hunk `4096`, unit `512`
- FAT32-LBA, partition LBA `63`, data LBA `19641`, cluster count `1,250,838`
- 내부 실행 파일 `EZ2DJ/EZ2DJ.EXE`, size `1,216,512`
- PE32/i386, image base `0x00400000`, entry RVA `0x00642240`, sections `6`

### 검증

- `cmd /c scripts\\build_win32.bat` 성공
- `ctest --test-dir build/windows-x86 -C Debug -R "re2dj_unit_tests|re2dj_windows_product_loader_probe" --output-on-failure` 성공, 2/2
- `re2dj.exe ez2dj3rd --list-targets` 성공
- 실제 shortcut 출력에서 다음을 확인했습니다.

  ```text
  chd image   : E:\\MYWORK\\Projects\\re2DJ\\roms\\ez2dj3rd\\ez2dj3rd.chd
  filesystem  : FAT32 label=NO NAME data_lba=19641 clusters=1250838
  * ez2dj3rd               EZ2DJ/EZ2DJ.EXE          built-in
  ```

이번 검증은 `--list-targets` 경로이므로 원본 게임의 전체 실행 성공이나 3rd 보호 응답의 유효성을 주장하지 않습니다. 확인 범위는 CHD 선택, FAT32 조회, 내부 PE 대표 경로입니다.

### 보존한 변경

작업 시작 전부터 존재한 `config/ez2dj-io.example.ini`의 사용자 변경은 스테이징하거나 커밋하지 않았습니다.

## English

### Work performed

- Changed the built-in `ez2dj3rd` profile from an extracted-directory HDD input to a MAME CHD input.
- Kept the default search path at `roms/ez2dj3rd` and selected `EZ2DJ/EZ2DJ.EXE` as the representative executable inside the CHD.
- Preserved the existing 3rd VFS, DirectSound, dynamic resolver, active-console, Hardlock, and detached-run policies.
- Updated the target-profile test so an extracted directory is not incorrectly claimed by the image-backed 3rd profile.
- Did not add the original CHD, IMG, or executable assets to the repository.

### Confirmed CHD structure

The user-supplied `roms/ez2dj3rd/ez2dj3rd.chd` was read with the CHD probe. It reports CHD v5, 10,262,568,960 logical bytes, 4,096-byte hunks, 512-byte units, a FAT32-LBA volume with partition LBA 63 and data LBA 19,641, and 1,250,838 clusters. The internal executable is `EZ2DJ/EZ2DJ.EXE`, 1,216,512 bytes, PE32/i386, image base `0x00400000`, entry RVA `0x00642240`, with six sections.

### Verification

- `cmd /c scripts\\build_win32.bat` succeeded.
- Focused CTest passed 2/2 tests.
- `re2dj.exe ez2dj3rd --list-targets` succeeded.
- The shortcut printed the supplied CHD path and selected `EZ2DJ/EZ2DJ.EXE`.

Because this verification used `--list-targets`, it does not claim full original-game execution or a valid 3rd protection response. It confirms CHD selection, FAT32 lookup, and the internal PE representative path.

### Preserved change

The pre-existing user modification to `config/ez2dj-io.example.ini` was left unstaged and uncommitted.
