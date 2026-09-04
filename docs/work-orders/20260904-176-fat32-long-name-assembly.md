# Task 176: FAT32 긴 이름 조립 수정

## 작업 목표

13자를 넘는 FAT32 긴 이름 뒤에 4바이트가 끼어드는 결함을 고치고, 원본 이미지 없이 재현되는 단위 시험으로 고정합니다.

## 선행 문서

- [Task 176 설계](../design/20260904-176-fat32-long-name-assembly.md)
- [Task 175 작업 로그](../work-logs/20260904-175-ez2dj4th-uninitialized-out-parameter.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **이름 해독 단위 분리.** `include/re2dj/storage/fat32_directory_name.h`와 `src/storage/fat32_directory_name.cpp`를 만들고 `DecodeFatShortName`, `FatShortNameChecksum`, `FatLongNameAssembler`를 옮깁니다.
2. **범위 수정.** 긴 이름 슬롯의 문자 범위를 `{{1, 5}, {14, 6}, {28, 2}}`로 고칩니다.
3. **호출 지점 정리.** `src/storage/fat32_chd.cpp`가 새 단위를 사용하도록 하고, 옮긴 정의를 제거합니다.
4. **단위 시험.** `tests/unit/fat32_directory_name_test.cpp`를 추가하고 설계 5절의 여섯 경우를 다룹니다. 합성 항목만 사용합니다.
5. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 갱신합니다. `ARCHITECTURE.md`에 새 단위를 반영합니다.

## 비범위

- VFS 현재 디렉터리 추적.
- 쓰기 지원이나 FAT 기록 변경.
- 짧은 이름 규칙 변경.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
.\build\windows-x86\bin\Debug\re2dj_chd_probe.exe roms\ez2dj4th\4thTrax.chd --list EZ2DJ/SYSTEM/Common
```

## 자기 검증 기준

- 새 단위 시험이 수정 전 코드에서 실패하고 수정 후 통과해야 의미가 있습니다. 13자 경우를 반드시 포함합니다.
- `--list` 결과에서 `1PLAYERInsertCoin.str`처럼 13자를 넘는 이름에 끼어든 문자가 없어야 합니다.
- 로그와 문서에는 파일 이름과 크기 같은 구조 정보만 남기고 파일 내용은 남기지 않습니다.

---

# Task 176: Fixing FAT32 Long-Name Assembly

## Goal

Fix the four bytes spliced into FAT32 long names past thirteen characters, and pin the behavior with a unit test that needs no original image.

## Scope

1. Move short-name decoding, the short-name checksum, and long-name assembly into `include/re2dj/storage/fat32_directory_name.h` and `src/storage/fat32_directory_name.cpp`.
2. Correct the slot character ranges to `{{1, 5}, {14, 6}, {28, 2}}`.
3. Make `src/storage/fat32_chd.cpp` use the new unit and drop the moved definitions.
4. Add `tests/unit/fat32_directory_name_test.cpp` covering the design's six cases with synthetic entries only.
5. Update the work log, the analysis topic, and `ARCHITECTURE.md`.

## Out of Scope

Guest working-directory tracking, write support, and short-name rule changes.

## Minimum Verification

Build, unit tests, the product loader probe, and a `--list` of `EZ2DJ/SYSTEM/Common` when a real image is present.

## Self-Check

The new test must fail against the old ranges and pass after the fix, and it must include the exactly-thirteen-character case. The listing must show no spliced characters in names such as `1PLAYERInsertCoin.str`. Documents record structural facts such as names and sizes, never file contents.
