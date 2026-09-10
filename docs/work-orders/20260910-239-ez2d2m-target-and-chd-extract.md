# 작업 지시서: ez2d2m target 추가와 CHD 재귀 추출

## 한국어

### 관련 설계

[ez2d2m target 추가와 CHD 재귀 추출 설계](../design/20260910-239-ez2d2m-target-and-chd-extract.md)

### 작업 항목

1. `re2dj_chd_probe`에 `--extract <내부 경로> <출력 디렉터리>`를 추가합니다. 빈 내부 경로는 볼륨 루트입니다.
2. 재귀 추출을 `chd_probe/main.cpp`가 아니라 별도 파일로 분리합니다. 이름 붙일 수 있는 하위 시스템이므로 `main`에는 인자 해석과 보고만 남깁니다.
3. 파일 읽기 실패는 세고 보고한 뒤 계속 진행하며, 하나라도 실패했으면 0이 아닌 종료 코드를 돌려줍니다.
4. `GetBuiltInTargetProfiles()`에 `ez2d2m` 항목을 설계 문서의 표대로 추가합니다. `MakeChdCompatibilityProfile()`은 쓰지 않습니다.
5. `legacy_io_ports`와 `legacy_io_ports_default`는 false로 두고, 이유를 프로파일 주석과 `note`에 남깁니다.
6. `ez2d2m` 프로파일을 확인하는 단위 시험을 추가하고, 기존 프로파일 시험이 그대로 통과하는지 확인합니다.
7. `docs/analysis/ez2d2m-chd-filesystem.md`를 작성합니다.
8. `docs/analysis/ez2dancer-io-map.md`를 작성하고, 공개 구현 인용은 GPL 코드 복사가 아닌 protocol 관찰로 남깁니다.
9. `docs/analysis/README.md` 색인을 갱신합니다.
10. `ARCHITECTURE.md`의 프로파일 표에 `ez2d2m`을 추가합니다.
11. Windows x86 build와 CTest를 검증합니다.
12. `roms/ez2d2m/ez2d2m.chd`를 `roms/ez2d2m/extracted`로 추출합니다.
13. 작업 로그를 작성합니다.

### 제외 범위

- 16비트 폭 legacy I/O bus와 `0x300` 대역 지원
- EZ2Dancer Hardlock 응답, descriptor, 실제 실행 성공
- 암호화된 `EZ2DANCER.ini`·`upgrade.ini`·`song.ini` 형식 해석
- CHD 볼륨 쓰기

### 완료 조건

- `--extract`가 이미지 전체를 펼치고, 읽지 못한 항목의 경로를 보고합니다.
- `FindBuiltInTargetProfileById("ez2d2m")`가 설계대로의 프로파일을 돌려줍니다.
- 기존 6개 프로파일 값이 변하지 않습니다.
- Windows x86 build와 CTest가 통과합니다.
- 추출본은 `roms/` 아래에만 있고 저장소에 커밋되지 않습니다.

## English

### Related design

[ez2d2m Target and Recursive CHD Extraction Design](../design/20260910-239-ez2d2m-target-and-chd-extract.md)

### Work items

1. Add `--extract <inner path> <output directory>` to `re2dj_chd_probe`, with an empty inner path meaning the volume root.
2. Put the recursive extraction in its own file rather than in `chd_probe/main.cpp`; it is a nameable subsystem, so `main` keeps only argument handling and reporting.
3. Count and report file-read failures, continue the walk, and return a non-zero exit code if anything failed.
4. Add the `ez2d2m` entry to `GetBuiltInTargetProfiles()` exactly as the design's table describes, written out rather than built from `MakeChdCompatibilityProfile()`.
5. Leave `legacy_io_ports` and `legacy_io_ports_default` false, with the reason recorded in the profile comment and its `note`.
6. Add a unit test covering the `ez2d2m` profile and confirm the existing profile tests still pass.
7. Write `docs/analysis/ez2d2m-chd-filesystem.md`.
8. Write `docs/analysis/ez2dancer-io-map.md`, citing the public implementation as an observed protocol rather than copied GPL code.
9. Update the `docs/analysis/README.md` index.
10. Add `ez2d2m` to the profile table in `ARCHITECTURE.md`.
11. Verify the Windows x86 build and CTest.
12. Extract `roms/ez2d2m/ez2d2m.chd` into `roms/ez2d2m/extracted`.
13. Write the work log.

### Out of scope

- A 16-bit-wide legacy I/O bus and the `0x300` port band
- The EZ2Dancer Hardlock response, its descriptor, and actual execution success
- The format of the encrypted `EZ2DANCER.ini`, `upgrade.ini` and `song.ini`
- Writing to a CHD volume

### Completion criteria

- `--extract` lays out the whole image and reports the path of anything it could not read.
- `FindBuiltInTargetProfileById("ez2d2m")` returns the designed profile.
- The six existing profiles are unchanged.
- The Windows x86 build and CTest pass.
- The extraction exists only under `roms/` and is never committed.
