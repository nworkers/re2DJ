# 작업 로그 200: ez2dj1st 타깃 프로파일

## 결과

사용자가 대표 실행 파일로 지정한 `Ez2DJ.exe`를 위해 `ez2dj1st` built-in target profile을 추가했다. 프로파일은 다른 HDD 실행 파일이나 보조 항목을 식별 근거로 사용하지 않는다. 대소문자 무시 파일명 비교에서 기존 `ez2dj.exe` 프로파일과 충돌하지 않도록 대표 실행 파일 자체의 PE header인 entry point RVA와 `SizeOfImage`를 함께 확인한다.

`ez2dj1stse`의 HLE 기본값은 요청대로 재사용했다. 새 바이너리의 raw I/O·보호 응답·게스트 부팅 경로가 동일하다는 뜻은 아니며, 독립 실행으로 확인되기 전까지 호환성 가정으로 남긴다.

## 확인된 사실

- 입력 경로 `roms/ez2dj1st/ez2dj/Ez2DJ.exe`가 사용자가 지정한 대표 실행 파일이다.
- PE32, i386, image base `0x00400000`, entry point RVA `0x0199b240`, `SizeOfImage 0x019b6000`, 6개 섹션, `.protect` 진입점을 확인했다.
- `ez2dj1st` fingerprint는 `Ez2DJ.exe`와 그 파일의 PE header 조건만 사용한다.
- `System.ini`가 확인되지 않아 guest drive letter와 guest directory는 설정하지 않았다.
- 원본 HDD 파일과 실행 파일은 저장소에 추가하거나 삭제하지 않았다.

## 구현

- `TargetFingerprint`에 선택적 PE header 조건을 추가했다.
- `MatchBuiltInTargetProfiles`가 이름, PE header, 필요한 경우 기존 프로파일의 형제 항목을 모두 확인하도록 했다.
- 실제 실행 파일의 중첩 경로에서 `ez2dj` working directory를 자동으로 설정한다.
- 합성 fixture에 대표 실행 파일 PE header를 반영해 `ez2dj1st`와 1st SE의 이름 충돌 회귀를 검증했다.
- 설계·작업 지시·분석·아키텍처 문서를 현재 대표 실행 파일 정책에 맞게 갱신했다.

## 검증

다음 검증을 통과했다.

```
cmd /c scripts\build_win32.bat
Windows Debug build: PASS

build\windows-x86\bin\Debug\re2dj_unit_tests.exe
checks: 1290, failures: 0

ctest --test-dir build\windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure
1/1 test passed

re2dj.exe ez2dj1st --list-targets
default: ez2dj1st -> ez2dj/Ez2DJ.exe

git diff --check
PASS
```

## 미확정 사항

`ez2dj1stse`에서 복제한 raw-I/O RVA, target-state, HLE 실행 정책이 이 `Ez2DJ.exe`에서도 정확히 일치하는지는 별도 coin/gameplay 실행으로 확인해야 한다. 이번 작업은 프로파일 선택과 실행 경로 연결까지만 검증했다.

---

# Work Log 200: ez2dj1st Target Profile

## Result

Added the `ez2dj1st` built-in target profile for the user-designated representative executable `Ez2DJ.exe`. The profile does not use another HDD executable or auxiliary entry as identification evidence. Because case-insensitive filename matching would collide with the existing `ez2dj.exe` profile, matching also checks the representative executable's own PE header: its entry point RVA and `SizeOfImage`.

The profile reuses the `ez2dj1stse` HLE defaults as requested. This does not establish that the new binary has the same raw-I/O, protection-response, or guest boot contract; those remain compatibility assumptions until independently run.

## Confirmed facts

- The user-designated representative is `roms/ez2dj1st/ez2dj/Ez2DJ.exe`.
- The image is PE32/i386, based at `0x00400000`, with entry point RVA `0x0199b240`, `SizeOfImage 0x019b6000`, six sections, and an entry point in `.protect`.
- The `ez2dj1st` fingerprint uses only `Ez2DJ.exe` and that executable's PE header constraints.
- No `System.ini` was confirmed, so the guest drive letter and guest directory remain unset.
- No original HDD file or executable was added to or deleted from the repository.

## Implementation

- Added optional PE header constraints to `TargetFingerprint`.
- Updated built-in matching to check the filename, PE header, and existing sibling constraints where applicable.
- Derives the `ez2dj` working directory from the matched executable's nested path.
- Added a synthetic fixture with the representative PE header so the `ez2dj1st`/1st SE filename collision is covered by regression tests.
- Updated the design, work-order, analysis, and architecture documentation.

## Verification

The Windows Debug build, 1,290 unit checks, selected CTest, real `--list-targets` shortcut, and `git diff --check` all passed. The real target list selected `ez2dj1st -> ez2dj/Ez2DJ.exe` as the default.

## Unresolved

Whether the copied raw-I/O RVA, target-state, and HLE execution policy exactly match this `Ez2DJ.exe` still requires an independent coin/gameplay run. This task verified profile selection and execution-path wiring only.
