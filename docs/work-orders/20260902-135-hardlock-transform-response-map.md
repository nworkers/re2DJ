# Hardlock transform 응답 매핑 작업 지시

관련 설계: [Hardlock transform 응답 매핑](../design/20260902-135-hardlock-transform-response-map.md)

*Related design: [Hardlock transform response map](../design/20260902-135-hardlock-transform-response-map.md).*

## 범위

1. 플랫폼 중립 `hardlock_transform_responses` parser를 추가합니다. 한 줄에 입력 16 hex와 출력 16 hex, `#` 주석과 빈 줄을 허용하고 중복 입력을 거절합니다.
2. `HardlockStubDevice`에 매핑 옵션을 추가합니다. `0x458`의 각 block을 조회해 맞으면 출력에 쓰고, 없으면 기존 항등 통과를 유지합니다.
3. injected runtime에 매핑 buffer와 개수 export를 추가하고 스텁에 연결합니다.
4. `0x458` block payload를 기록하는 명시적 진단 옵션을 추가합니다. 기본은 비기록입니다.
5. launcher에 매핑 파일 옵션과 입력 기록 옵션을 추가합니다.
6. unit test로 parser와 스텁 조회 계약을 고정합니다.
7. 4th 실제 실행으로 입력 목록을 두 번 수집해 동일성을 확인하고, `.text` 구간 시작과 대조합니다.
8. 합성 매핑 주입 실행으로 주입 경로가 동작함을 확인합니다.
9. 문서를 갱신하고 작업 로그를 남깁니다.

*Add a platform-neutral `hardlock_transform_responses` parser accepting one line per entry with sixteen hex digits of input and output, allowing `#` comments and blank lines while rejecting duplicate inputs; add the map option to `HardlockStubDevice` so each `0x458` block is looked up and written when found with the existing identity passthrough otherwise; add the map buffer and count exports to the injected runtime and wire them to the stub; add an explicit diagnostic that records `0x458` block payloads, off by default; add the launcher options for the map file and input recording; pin the parser and lookup contract with unit tests; collect the 4th input list twice from real runs to confirm it is identical and compare it against `.text` chunk starts; confirm the injection path works with a synthetic map; and update the documents with a work log.*

## 비범위

- Function `0x0e` 변환 구현. 이 저장소에서 하지 않습니다.
- 외부 응답 계산 프로그램과의 실행 중 통신.
- seed 탐색.

*Out of scope: implementing the Function `0x0e` transform in this repository, communicating with the external response-computing program at run time, and seed search.*

## 완료 조건

- 매핑 파일 없이 실행하면 기존 동작이 그대로입니다.
- 매핑을 준 실행이 trace에 조회 성공 여부를 남깁니다.
- 4th 입력 목록이 두 실행에서 동일합니다.
- 매핑 파일이 Git 추적 대상에 나타나지 않습니다.
- unit test 전체와 Windows x86 build가 통과합니다.

*Completion requires unchanged behavior without a map file, a trace recording lookup success for a run given a map, an identical 4th input list across two runs, no map file appearing in Git tracking, and a passing unit-test suite and Windows x86 build.*
