# EZ2DJ 1st SE 제품명 표기 교정 설계

## 상태

**구현 완료.** 1st SE의 사람 대상 제품명을 `EZ2DJ 1st Trax Special Edition`에서 `EZ2DJ The 1st Tracks Special Edition`으로 통일했다. 내부 target ID, 실행 파일명, 경로, 감지 규칙은 변경하지 않았다.

* **Implemented.** The user-facing 1st SE product name was normalized from `EZ2DJ 1st Trax Special Edition` to `EZ2DJ The 1st Tracks Special Edition`. Internal target IDs, executable names, paths, and detection rules remain unchanged.

## 범위

- built-in target profile의 `display_name`과 unprotected bring-up profile의 표시 이름을 교정한다.
- README, 실행 파일 설계 문서, HDD 분석 문서, 작업 이력처럼 해당 제품명을 설명하는 저장소 문구를 같은 표기로 갱신한다.
- target profile 단위 테스트에서 canonical profile의 표시 이름을 고정한다.
- 3rd 제품의 기존 `EZ2DJ 3rd Trax` 표기는 이번 변경 대상이 아니므로 유지한다.

* Update the built-in target profile display name and the unprotected bring-up profile's display name.
* Update repository prose that names this product, including the README, executable-design document, HDD analysis, and task history.
* Pin the canonical profile's display name in the target-profile unit test.
* Keep the existing `EZ2DJ 3rd Trax` name because it is outside this request.

## 설계 원칙

제품명 표기만 변경하며, `ez2dj1stse`를 비롯한 기계 판독용 값은 보존한다. 따라서 target detection과 launch behavior에는 영향을 주지 않고, 목록·창 제목 등 `display_name`을 소비하는 사용자 노출 지점만 새 이름을 사용한다.

*Change the product label only and preserve machine-readable values such as `ez2dj1stse`. Target detection and launch behavior therefore remain unchanged; only user-facing locations that consume `display_name` use the new name.*

## 검증

1. 저장소 전체에서 1st SE의 이전 표기가 남아 있지 않고, 3rd의 `Trax`는 유지되는지 검색한다.
2. Windows x86 대상 빌드와 CTest를 실행한다.
3. target profile 단위 테스트가 canonical profile의 새 `display_name`을 통과하는지 확인한다.

*Verification consists of searching the repository for stale 1st SE wording while confirming that 3rd's `Trax` remains, running the Windows x86 build and CTest, and checking the canonical profile display-name assertion.*
