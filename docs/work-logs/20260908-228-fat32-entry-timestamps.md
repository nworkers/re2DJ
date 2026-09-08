# 작업 로그: FAT32 항목 타임스탬프와 열거 결과

## 한국어

### 관련 문서

- 설계: [FAT32 항목 타임스탬프와 열거 결과 설계](../design/20260908-228-fat32-entry-timestamps.md)
- 작업 지시: [FAT32 항목 타임스탬프와 열거 결과](../work-orders/20260908-228-fat32-entry-timestamps.md)
- 선행 작업: [ez2dj1stse 스프라이트 적재 경계 관측](20260908-227-ez2dj1stse-sprite-load-boundary.md)

### 수행 내용

`Fat32Entry`에 DOS 형식 생성 시각·생성 날짜·마지막 접근 날짜·쓰기 시각·쓰기 날짜를 추가하고, FAT32 reader가 디렉터리 항목의 오프셋 `0x0E`, `0x10`, `0x12`, `0x16`, `0x18`에서 읽도록 했습니다. 값은 변환하지 않고 원형으로 싣습니다. `src/storage`는 플랫폼 공용 코어라 `FILETIME`을 쓸 수 없기 때문입니다.

`PopulateFindData`가 `DosDateTimeToFileTime`으로 변환해 `WIN32_FIND_DATAA`의 `ftCreationTime`, `ftLastAccessTime`, `ftLastWriteTime`을 채웁니다. 날짜 워드가 0인 항목은 변환하지 않고 0으로 둡니다. `DosDateTimeToFileTime`이 0을 거부하고, 시간이 없는 항목과 채우지 못한 항목을 구분할 필요가 없기 때문입니다.

`re2dj_chd_probe --list` 출력에 해독한 날짜·시각을 더했습니다. 합성 FAT32 볼륨 픽스처가 없어 단위 시험으로 이 경로를 고정할 수 없었고, 대신 실제 이미지로 확인할 수 있게 한 것입니다.

### 검증 — 값 적재

`re2dj_chd_probe roms\ez2dj1stse\ez2dj1stse.chd --list "ez2dj/System/Title"` 결과입니다.

| 항목 | 쓰기 | 생성 | 접근 |
| --- | --- | --- | --- |
| `flare.bmp` | 1999-06-09 17:01 | 1999-12-20 | 1999-01-20 |
| `amuse.bmp` | 1999-06-09 17:12 | 1999-12-20 | 2015-04-25 |
| `bg.bmp` | 1999-11-23 21:06 | 1999-12-20 | 2015-04-25 |
| `bg1.bmp` | 1999-07-14 15:08 | 1999-12-20 | 1999-01-20 |

1999년 제품의 자산으로 타당한 값이고, 이전에는 세 필드가 모두 0이었습니다. 접근 날짜 일부가 2015년인 것은 덤프가 이후에 열린 흔적으로 보이며 이 작업에서 판단하지 않습니다.

### 검증 — 가설

**후보 1은 배제되었습니다.** 타임스탬프를 채운 뒤 같은 실행을 반복했지만 화면은 그대로입니다. 스프라이트 자리에 여전히 이름표 사각형이 그려집니다.

trace도 동일합니다. `.bmp` 61개가 전부 `flags=0x00000080`으로 한 번만 열리고, `file-query`(`GetFileSize`/`GetFileType`)는 `.bmp` 핸들에 한 건도 발생하지 않으며, 읽기는 `.wav`와 `.str`로만 갑니다.

따라서 게스트의 스프라이트 적재 결정은 **디렉터리 열거 결과의 시간 필드를 보지 않습니다.** [작업 227](20260908-227-ez2dj1stse-sprite-load-boundary.md)의 세 후보 중 하나가 확실하게 제거되었습니다.

이 변경 자체는 가설과 무관하게 유지합니다. `FindFirstFileA`가 0을 돌려주던 것은 원본 이미지의 사실과 다르고, 시간을 읽는 다른 게스트 코드도 같은 영향을 받기 때문입니다.

### 검증 — 회귀

- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0
- 3rd·4th를 40초 제한으로 실행. 두 경우 모두 정상 실행 중이었고, VFS trace 앞 120줄이 이전 실행과 handle·tick 값을 제외하고 동일합니다.

### 남은 후보

1. ~~열거 결과의 타임스탬프~~ — **배제됨**
2. `.str` 레코드가 담은 기대값(크기·치수)과 실제 값의 비교
3. 적재를 나중 단계로 미루는 정상 동작이고 그 단계에 도달하지 못하는 것

다음 확인 방법으로는 `title.str`의 레코드 구조를 해독해 스프라이트 항목이 크기나 치수를 담는지 보는 것이 가장 쌉니다. 4,060 바이트에 13개 이름이 있으므로 레코드 길이는 약 312 바이트이고, 이름 필드 주변의 고정 오프셋을 원본 BMP의 크기·치수와 대조하면 판별할 수 있습니다. 그것으로도 갈리지 않으면 존재 확인 직후 구간을 명령어 수준으로 추적합니다.

## English

### Related documents

- Design: [FAT32 Entry Timestamps and Enumeration Results Design](../design/20260908-228-fat32-entry-timestamps.md)
- Work order: [FAT32 Entry Timestamps and Enumeration Results](../work-orders/20260908-228-fat32-entry-timestamps.md)
- Preceding task: [ez2dj1stse sprite-load boundary observation](20260908-227-ez2dj1stse-sprite-load-boundary.md)

### What was done

`Fat32Entry` gained DOS-format creation time, creation date, last-access date, write time, and write date, and the FAT32 reader now reads them from directory-entry offsets `0x0E`, `0x10`, `0x12`, `0x16`, and `0x18`. They are carried unconverted because `src/storage` is platform-neutral core and cannot use `FILETIME`.

`PopulateFindData` converts them with `DosDateTimeToFileTime` to fill `ftCreationTime`, `ftLastAccessTime`, and `ftLastWriteTime` in `WIN32_FIND_DATAA`, leaving an entry whose date word is zero at zero, since `DosDateTimeToFileTime` rejects zero and there is no need to distinguish an entry with no stamp from one we could not fill.

The `re2dj_chd_probe --list` output now decodes the dates and times. No synthetic FAT32 volume fixture exists, so a unit test could not pin this path; exposing the values in the probe makes it verifiable against the real image instead.

### Verification — value loading

`re2dj_chd_probe roms\ez2dj1stse\ez2dj1stse.chd --list "ez2dj/System/Title"` reports `flare.bmp` written 1999-06-09 17:01 and created 1999-12-20, `amuse.bmp` written 1999-06-09 17:12, `bg.bmp` written 1999-11-23 21:06, and `bg1.bmp` written 1999-07-14 15:08. These are plausible for a 1999 product's assets, and all three fields previously read zero. Some access dates fall in 2015, which looks like later handling of the dump and is not judged here.

### Verification — hypothesis

**Candidate one is eliminated.** Repeating the run with the timestamps in place leaves the screen unchanged: labeled rectangles still stand where the sprites belong.

The trace is unchanged too. All 61 `.bmp` files are opened once with `flags=0x00000080`, no `file-query` event (`GetFileSize` or `GetFileType`) occurs on any `.bmp` handle, and reads reach only `.wav` and `.str` files.

The guest's sprite-load decision therefore **does not read the time fields of the enumeration result**, removing one of the three candidates from [task 227](20260908-227-ez2dj1stse-sprite-load-boundary.md).

The change is kept regardless of the hypothesis: returning zero from `FindFirstFileA` misreported the original image, and any other guest code that reads file times was affected the same way.

### Verification — regression

`re2dj_unit_tests.exe` reported `checks: 1421, failures: 0` and `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0. 3rd and 4th were each run under a 40-second bound, both still running at the bound, with their first 120 VFS trace lines identical to the previous runs apart from handle and tick values.

### Remaining candidates

Candidate one, the enumeration timestamps, is eliminated. The remaining two are a comparison against expected size or dimensions carried in the `.str` record, and a normal deferral of loading to a later stage the run never reaches.

The cheapest next check is to decode the `title.str` record layout and see whether a sprite entry carries a size or dimensions: 4,060 bytes hold 13 names, so records are about 312 bytes, and fixed offsets around the name field can be compared against the original bitmaps' sizes and dimensions. If that does not separate them, trace the instructions immediately after the existence check.
