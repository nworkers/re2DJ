# 작업 지시 200: ez2dj1st 타깃 프로파일 추가

## 목표

`roms/ez2dj1st/ez2dj`에 배치된 EZ2DJ The 1st Tracks 실행 파일을 `ez2dj1st` built-in target profile로 인식하고, 기존 `ez2dj1stse`와 동일한 기본 HLE 실행 경로를 사용하도록 한다.

## 선행 문서

- [Task 200 설계](../design/20260906-200-ez2dj1st-target-profile.md)
- [Built-in target profile 설계](../design/20260822-005-built-in-target-profiles.md)
- [HDD layout analysis](../analysis/ez2dj-hdd-layout.md)
- [Target profile implementation](../../src/target/target_profile.cpp)

## 범위

1. `ez2dj1st` built-in profile을 target profile 테이블에 추가한다.
2. 사용자가 준비한 `Ez2DJ.exe`를 대표 실행 파일로 하고, 이 파일의 이름과 자체 PE header(entry point RVA, SizeOfImage)만 fingerprint로 사용한다.
3. `roms/ez2dj1st` shortcut 경로와 `ez2dj` working-directory mount를 연결한다.
4. `ez2dj1stse`의 HLE 기본값을 복제하되, guest drive/directory는 `System.ini` 부재로 비워 둔다.
5. unit test에서 중첩 HDD layout, profile 선택, 실행 기본값을 검증한다.
6. 관련 analysis/설계 문서와 `EXE_DESIGN.ko.md`/`.en.md`를 갱신한다.

## 범위 밖

- 원본 HDD 파일, 실행 파일, 게임 데이터를 저장소에 추가하지 않는다.
- Asia/Japan 지역 실행 파일의 별도 실행 프로파일을 확정하지 않는다.
- 새 바이너리의 raw-I/O 주소, 보호 응답, 화면/입력 동작을 독립적으로 확정하지 않는다.
- 게임 로직이나 원본 실행 파일을 수정하지 않는다.

## 구현 계획

1. `target_profile.cpp`에 1st profile entry를 추가한다.
2. target profile unit test에 실제 구조를 모사한 `ez2dj` 하위 루트 fixture를 추가한다.
3. `docs/analysis/ez2dj-hdd-layout.md`, `docs/EXE_DESIGN.*`, 관련 색인을 새 입력의 확인 사실과 미확정 사항에 맞춰 갱신한다.
4. Windows Debug build, unit test, 선택 CTest, 실제 `--list-targets` shortcut을 검증한다.
5. 결과와 compatibility assumption을 작업 로그에 기록한다.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
ctest --test-dir build\windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure
.\build\windows-x86\bin\Debug\re2dj.exe ez2dj1st --list-targets
git diff --check
```

## Work Order 200: Add the ez2dj1st Target Profile

### Goal

Recognize the EZ2DJ The 1st Tracks executable staged under `roms/ez2dj1st/ez2dj` as the `ez2dj1st` built-in target profile and route it through the same baseline HLE execution path as `ez2dj1stse`.

### Scope

Add one directory-backed profile keyed only by the user-prepared `Ez2DJ.exe` and that executable's PE header identity. Use `roms/ez2dj1st` as the shortcut directory, derive the `ez2dj` working directory from the matched executable path, and copy the 1st SE HLE defaults. Leave guest drive/path empty because no `System.ini` was found. Add fixture coverage and update the project analysis and design indexes without storing or deleting the original assets.

### Out of scope

Do not add Asia/Japan profiles, claim their raw-I/O or protection values as independently verified, modify original assets or executable code, or reimplement game logic.

### Verification

Run the Windows Debug build, the target-profile unit test and CTest, the real `ez2dj1st --list-targets` shortcut, and `git diff --check`. Record the result and the compatibility assumption in the work log.
