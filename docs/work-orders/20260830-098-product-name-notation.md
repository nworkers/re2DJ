# EZ2DJ 1st SE 제품명 표기 교정 작업 지시

## 목적

1st SE의 제품명을 정확한 공식 표기인 `EZ2DJ The 1st Tracks Special Edition`으로 표시한다. 현재 내부 식별자와 실행 동작은 유지한다.

*Display the 1st SE product using the exact name `EZ2DJ The 1st Tracks Special Edition` while preserving current internal identifiers and execution behavior.*

## 작업

1. 관련 설계 문서를 작성한다.
2. `src/target/target_profile.cpp`의 1st SE 두 profile 표시명을 교정한다.
3. canonical profile 단위 테스트에 정확한 표시명 검증을 추가한다.
4. README와 1st SE를 설명하는 문서·작업 이력의 표기를 통일한다. 3rd의 `Trax`는 변경하지 않는다.
5. 이전 표기 검색, Windows x86 빌드, CTest 결과를 작업 로그에 기록한다.

*Tasks: write the design document; correct both 1st SE profile display names; add an exact display-name assertion; normalize README and 1st SE documentation while leaving 3rd's `Trax` unchanged; and record stale-wording search, Windows x86 build, and CTest results in the work log.*

## 제외 범위

- `ez2dj1stse` 등 target ID 변경
- `ez2dj.exe`, `ez2dj1.exe` 등 원본 파일명 변경
- target detection, launch policy, gameplay, renderer 동작 변경
- 원본 HDD·실행 파일·게임 자산 수정 또는 저장

*Excluded: changing target IDs or original filenames, changing detection/launch/gameplay/renderer behavior, or modifying/storing original HDD contents, executables, or game assets.*

## 완료 조건

- 1st SE 관련 표시가 모두 `EZ2DJ The 1st Tracks Special Edition` 또는 문맥에 맞는 `The 1st Tracks Special Edition`으로 교정된다.
- 3rd 제품의 `EZ2DJ 3rd Trax` 표기는 유지된다.
- 빌드와 CTest가 통과한다.
- 대응 설계·작업 지시·작업 로그 문서가 저장소에 남는다.

*Completion requires all 1st SE labels to use `EZ2DJ The 1st Tracks Special Edition` or its context-appropriate shortened form, 3rd's `EZ2DJ 3rd Trax` to remain unchanged, build and CTest success, and the corresponding design, work-order, and work-log documents.*
