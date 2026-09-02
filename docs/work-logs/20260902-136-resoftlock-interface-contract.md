# reSoftlock 인터페이스 계약 작업 로그

관련 설계: [reSoftlock 인터페이스 계약](../design/20260902-136-resoftlock-interface-contract.md)

*Related design: [reSoftlock interface contract](../design/20260902-136-resoftlock-interface-contract.md).*

## 결과

- re2DJ가 Hardlock 응답 생성기에게 요구하는 입출력 계약을 정의했습니다. 코드 변경은 없습니다.
- 계약은 파일 하나로만 접촉하는 경계를 규정합니다. 파이프, 소켓, 공유 메모리, 링크를 쓰지 않고 불투명한 16진 바이트 열만 주고받습니다.
- 응답 매핑 파일 형식을 re2DJ parser가 강제하는 규칙 그대로 기술했습니다. 16 hex challenge와 16 hex response, `#` 주석, 중복 challenge 거절, 빈 매핑 거절입니다.
- challenge 목록 파일 형식과 seed 후보 목록 파일 형식을 정의했습니다.
- 요구 실행 모드 네 가지를 정의했습니다. `map`, `map-batch`, `seeds`, 선택적 `challenges`입니다. 후보가 수십 개일 수 있으므로 `map-batch`가 실질적으로 가장 중요합니다.
- 비밀값 취급, 결정성, 중복 접기, 종료 코드, 자체 검증 요구사항을 명시했습니다.
- 확인된 ez2dj4th 파라미터와 판별 루프를 함께 기록했습니다. fault 주소를 판별 기준으로 쓰지 않는다는 제약도 계약에 포함했습니다.

*Defined the input/output contract re2DJ requires from a Hardlock response generator, with no code change. The contract specifies a boundary that meets through a single file — no pipes, sockets, shared memory, or linking — exchanging only opaque hexadecimal byte strings. It states the response map format exactly as re2DJ's parser enforces it: sixteen hex digits of challenge and response, `#` comments, rejection of duplicate challenges, and rejection of an empty map. It defines the challenge list and seed candidate list formats, and four required modes — `map`, `map-batch`, `seeds`, and an optional `challenges` — with `map-batch` mattering most because candidates may number in the dozens. It also states secret handling, determinism, duplicate folding, exit codes, and self-verification requirements, records the confirmed ez2dj4th parameters and the judgement loop, and carries the constraint that the fault address is not a judgement criterion.*

## Challenge 유도 규칙 검증

계약이 기술한 유도 규칙을 원본 실행 파일에 직접 적용해 관찰 목록과 대조했습니다.

규칙은 각 PE section에 대해 raw 시작에서 `0x8000` 간격으로 걷고 각 지점의 8바이트를 취하며, 다음 지점이 section raw 범위를 벗어나면 그 section을 끝내고, `.idata`와 `.protect`를 제외하는 것입니다.

- 유도 결과 36개, 관찰 36개
- **값과 순서까지 정확히 일치**

**확인됨.** reSoftlock은 re2DJ를 실행하지 않고도 원본 실행 파일만으로 challenge 목록을 만들 수 있습니다. 따라서 두 프로그램은 challenge 목록 파일조차 주고받지 않아도 되며, 경계는 응답 매핑 파일 하나로 줄어듭니다.

*Applying the documented derivation rule directly to the original executable and comparing with the observed list — walking each PE section from its raw start at `0x8000` intervals, taking eight bytes at each point, stopping when the next point leaves the section, and skipping `.idata` and `.protect` — produced 36 challenges against 36 observed, matching **exactly in both value and order**. **Confirmed:** reSoftlock can build the challenge list from the original executable alone without running re2DJ, so the two programs need not even exchange a challenge list and the boundary narrows to a single response map file.*

## 검증

- 유도 규칙이 관찰 목록을 값과 순서까지 재현
- 매핑 파일 형식 기술이 `hardlock_transform_responses` parser 구현 및 unit test와 일치
- 코드 변경이 없으므로 build 영향 없음
- `git diff --check` 통과

*Verification: the derivation rule reproduces the observed list in value and order, the documented map format matches the `hardlock_transform_responses` parser implementation and its unit tests, there is no build impact because no code changed, and `git diff --check` passes.*

## 계약 확장 — seed 복구 지침

사용자 질문에 따라 3절을 확장했습니다. seed 복구가 왜 생성기 쪽에만 있을 수 있는지(`F` 계산 능력이 필요하고 re2DJ에는 `F`가 없음), 전체 공간 블랙박스 열거가 2⁴⁸으로 현실성이 없다는 점, 권장하는 2단계 전략(중간 control 값 확정 후 좁아진 공간 탐색), 그리고 3rd를 회귀 픽스처로 쓰는 방법을 넣었습니다.

3rd는 `ID_Ref=478c8b793f201f8a`, `ID_Verify=cc22ae2da344b2a2`, module address `0x4c51`에 대해 기대 control 값 `74 6c 2c 1c f0`과 후보 11개 이상이라는 기대 결과가 이미 있습니다. 4th는 정답을 모르므로 자기 검증이 불가능하지만 3rd는 가능하므로, 생성기는 3rd를 먼저 재현한 뒤에만 4th 결과를 신뢰합니다. 출력 결정성, 중단 시 하한 표기, 로그 비노출, 후보 0개와 입력 오류의 종료 코드 구분도 함께 명시했습니다.

*Contract extension — seed recovery guidance. Section 3 was expanded per the user question: why recovery can live only on the generator side, since it needs the ability to compute `F` and re2DJ does not have it; why full-space black-box enumeration is impractical at 2⁴⁸; the recommended two-stage strategy of fixing the intermediate control value before scanning the narrowed space; and how to use 3rd as a regression fixture. 3rd already has expected results for `ID_Ref=478c8b793f201f8a`, `ID_Verify=cc22ae2da344b2a2`, and module address `0x4c51` — the control value `74 6c 2c 1c f0` and at least eleven candidates — so because 4th cannot self-verify while 3rd can, the generator reproduces 3rd first and only then trusts a 4th result. Output determinism, marking an interrupted search as a lower bound, keeping seeds out of logs, and separating a zero-candidate result from an input error by exit code were specified as well.*

## 다음 단계

1. 계약 문서를 reSoftlock 저장소에도 두어 양쪽이 같은 계약을 참조하게 합니다.
2. reSoftlock이 `seeds`와 `map-batch`를 구현하면 후보별 매핑 파일을 받아 판별 루프를 돌립니다.
3. 판별기는 복호화된 영역의 엔트로피와 x86 prologue 빈도로 채점합니다. fault 주소는 쓰지 않습니다.

*Next: mirror the contract in the reSoftlock repository so both sides reference the same document; once reSoftlock implements `seeds` and `map-batch`, take the per-candidate map files and run the judgement loop; and score candidates by the decrypted region's entropy and x86 prologue frequency, never by the fault address.*
