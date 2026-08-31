# EZ2DJ 1st SE 제품명 표기 교정 작업 로그

## 결과 요약

1st SE의 사용자 대상 표기를 `EZ2DJ The 1st Tracks Special Edition`으로 교정했다. canonical profile과 unprotected bring-up profile의 `display_name`을 모두 갱신했으며, `ez2dj1stse` target ID, 원본 실행 파일명, 감지·실행 동작은 변경하지 않았다.

README, `EXE_DESIGN` 한·영 문서, HDD/import 분석 문서, 관련 기존 작업 이력도 같은 표기로 통일했다. 3rd 제품의 `EZ2DJ 3rd Trax`는 요청 범위가 아니므로 유지했다.

*The user-facing 1st SE label is now `EZ2DJ The 1st Tracks Special Edition`. Both the canonical and unprotected bring-up profile `display_name` values were updated; the `ez2dj1stse` target ID, original executable names, and detection/launch behavior were unchanged.*

*The README, Korean and English `EXE_DESIGN` documents, HDD/import analysis, and related existing task history now use the same name. The 3rd product remains `EZ2DJ 3rd Trax` because it was outside the request.*

## 변경 파일

- `src/target/target_profile.cpp`
- `tests/unit/target_profile_test.cpp`
- `README.md`
- `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`
- `docs/analysis/ez2dj-hdd-layout.md`, `docs/analysis/ez2dj-import-surface.md`
- 관련 기존 작업 지시·로그 문서
- [설계 문서](../design/20260830-098-product-name-notation.md)
- [작업 지시서](../work-orders/20260830-098-product-name-notation.md)

*Changed source/test files, repository documentation and task history, plus the design and work-order documents linked above.*

## 검증

1. 이전 1st SE 표현을 저장소에서 검색했다. 설계 문서의 변경 전·후 설명을 제외하고 이전 표시가 남아 있지 않음을 확인했다.
2. `EZ2DJ The 1st Tracks Special Edition`이 source, test, README, 설계·분석 문서에 반영된 것을 확인했다.
3. `EZ2DJ 3rd Trax`가 유지된 것을 확인했다.
4. `cmake --build --preset windows-x86-debug --config Debug` 성공.
5. `ctest --preset windows-x86-debug` 성공 — 3/3.
6. `cmake --build --preset windows-x86-native-probe --config Debug` 성공.
7. `ctest --preset windows-x86-native-probe` 성공 — 1/1.

*Verification: the stale 1st SE wording was searched for; the new name was confirmed across source, tests, README, design, and analysis; `EZ2DJ 3rd Trax` was confirmed unchanged; the Windows x86 Debug build passed; the full CTest suite passed 3/3; and the native-probe Debug build and CTest passed 1/1.*

## 비고

이번 변경은 표시 문자열과 문서 표현만 다루므로 원본 HDD 자산이나 실행 파일은 읽기·수정 대상으로 삼지 않았다. 창 제목을 포함한 사용자 노출 경로는 profile `display_name` 소비 지점을 통해 새 표기를 사용한다.

*This change is limited to display strings and documentation, so no original HDD asset or executable was read or modified. User-facing paths, including the window title, receive the new wording through consumers of profile `display_name`.*
