# 작업 지시서: FAT32 항목 타임스탬프와 열거 결과

## 한국어

### 관련 설계

[FAT32 항목 타임스탬프와 열거 결과 설계](../design/20260908-228-fat32-entry-timestamps.md)

### 작업 항목

1. `Fat32Entry`에 DOS 형식 생성 시각·생성 날짜·마지막 접근 날짜·쓰기 시각·쓰기 날짜 필드를 추가합니다.
2. FAT32 reader가 디렉터리 항목에서 그 값들을 읽습니다.
3. `PopulateFindData`가 `DosDateTimeToFileTime`으로 변환해 `WIN32_FIND_DATAA`의 세 시간 필드를 채웁니다. 값이 0이면 0으로 둡니다.
4. 단위 시험에 DOS 값 적재를 고정합니다.
5. Windows x86 build와 시험을 검증합니다.
6. 1st SE 실행으로 스프라이트 적재 동작 변화를 관측합니다.
7. 3rd·4th 회귀를 확인합니다.
8. 분석과 작업 로그를 갱신합니다.

### 제외 범위

- 쓰기 경로나 overlay 타임스탬프
- `GetFileTime` HLE 추가
- 디렉터리 열거 이외 경로의 시간 보고

### 완료 조건

- 열거 결과가 원본 FAT32 시간을 담습니다.
- unit test와 VFS runtime probe가 통과합니다.
- 1st SE 관측 결과가 후보 1을 확정하거나 배제합니다.
- 3rd·4th에 회귀가 없습니다.

## English

### Related design

[FAT32 Entry Timestamps and Enumeration Results Design](../design/20260908-228-fat32-entry-timestamps.md)

### Work items

1. Add DOS-format creation time, creation date, last-access date, write time, and write date fields to `Fat32Entry`.
2. Read those values from the directory entry in the FAT32 reader.
3. Convert them with `DosDateTimeToFileTime` in `PopulateFindData` to fill the three `WIN32_FIND_DATAA` time fields, leaving a stored zero as zero.
4. Pin the DOS value loading in unit tests.
5. Verify the Windows x86 build and tests.
6. Observe the 1st SE run for a change in sprite-loading behavior.
7. Confirm no regression for 3rd and 4th.
8. Update the analysis document and the work log.

### Out of scope

- Write paths and overlay timestamps
- Adding a `GetFileTime` HLE
- Reporting times through paths other than directory enumeration

### Completion criteria

- Enumeration results carry the original FAT32 times.
- The unit tests and the VFS runtime probe pass.
- The 1st SE observation either confirms or eliminates candidate one.
- 3rd and 4th show no regression.
