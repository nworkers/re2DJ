# ez2dj3rd CHD 파일시스템 분석

## 한국어

### 확인된 구조

사용자가 제공한 `roms/ez2dj3rd/ez2dj3rd.chd`를 `re2dj_chd_probe`로 읽었습니다. 원본 CHD와 실행 파일은 저장소에 추가하지 않습니다.

- CHD v5, logical bytes `10,262,568,960`, hunk `4096`, unit `512`
- codecs `lzma,zlib,huff,flac`
- GDDD metadata `CYLS:19885,HEADS:16,SECS:63,BPS:512`
- FAT32 partition LBA `63`, partition sectors `20,032,992`
- sectors per cluster `16`, reserved sectors `32`, sectors per FAT `9773`
- data LBA `19641`, root cluster `2`, cluster count `1,250,838`
- volume label `NO NAME`
- 내부 실행 파일 `EZ2DJ/EZ2DJ.EXE`, first cluster `742547`, size `1,216,512`
- PE machine `i386`, magic `PE32`, subsystem `windows-gui`, image base `0x00400000`, entry RVA `0x00642240`, sections `6`

이 CHD는 현재 `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE`를 대상으로 하던 3rd profile과 동일한 대표 실행 파일 경로·크기·PE 식별값을 제공합니다. 전체 바이트 동일성이나 Hardlock 계약의 동일성은 이 probe만으로 확정하지 않습니다.

### 실행 연결

`ez2dj3rd` built-in profile은 이제 CHD shortcut으로 동작합니다. launcher는 CHD의 FAT32에서 `EZ2DJ/EZ2DJ.EXE`를 조회하고, Windows x86 original-process backend에 CHD 경로와 staging 실행 파일 경로를 함께 전달합니다. 기존 3rd HLE/Hardlock 정책은 그대로 유지됩니다.

## English

### Confirmed structure

The user-supplied `roms/ez2dj3rd/ez2dj3rd.chd` was read with `re2dj_chd_probe`. The original CHD and executable are not added to the repository.

- CHD v5, 10,262,568,960 logical bytes, 4,096-byte hunks, 512-byte units
- codecs `lzma,zlib,huff,flac`
- GDDD metadata `CYLS:19885,HEADS:16,SECS:63,BPS:512`
- FAT32 partition LBA 63 with 20,032,992 sectors
- 16 sectors per cluster, 32 reserved sectors, 9,773 sectors per FAT
- data LBA 19,641, root cluster 2, and 1,250,838 clusters
- volume label `NO NAME`
- `EZ2DJ/EZ2DJ.EXE`, first cluster 742,547, size 1,216,512 bytes
- PE i386/PE32 Windows GUI, image base `0x00400000`, entry RVA `0x00642240`, six sections

The CHD exposes the same representative executable path, size, and PE identity used by the previous extracted-directory 3rd profile. Complete byte identity and an identical Hardlock contract are not established by this probe alone.

### Execution connection

The built-in `ez2dj3rd` profile now uses the CHD shortcut. The launcher resolves `EZ2DJ/EZ2DJ.EXE` through the CHD FAT32 view and passes both the CHD path and staging executable path to the Windows x86 original-process backend. The existing 3rd HLE/Hardlock policy remains unchanged.
