# Hardlock 후보 판별 작업 지시

관련 가이드: [Hardlock seed 복구 워크스루](../guides/hardlock-seed-recovery-walkthrough.md) Stage 6·7

*Related guide: stages 6 and 7 of the [Hardlock seed recovery walkthrough](../guides/hardlock-seed-recovery-walkthrough.md).*

## 범위

1. reSoftlock이 산출한 후보별 응답 매핑 파일을 원본 실행에 주입합니다. 3rd 105개, 4th 93개입니다.
2. 각 실행에서 주입 완전성을 먼저 확인합니다. 3rd는 transform 32줄, 4th는 36줄이 모두 `mapped=1:unmapped=0`이어야 합니다.
3. 실행 형태를 기록합니다. 종료 코드, vfs trace 줄 수, 해석된 API 이름 수, 도달 경계입니다.
4. fault 주소는 판별 기준으로 쓰지 않습니다. 실행마다 달라지는 할당 주소입니다.
5. 나머지 후보와 다른 실행 형태를 보이는 후보를 찾습니다.
6. 후보가 하나로 좁혀지면 재실행으로 재현성을 확인합니다.
7. 결과를 분석 문서와 워크스루에 반영하고 작업 로그를 남깁니다.

*Inject each candidate response map produced by reSoftlock into the original execution — 105 for 3rd and 93 for 4th; confirm injection completeness first, requiring all 32 transform lines for 3rd and 36 for 4th to read `mapped=1:unmapped=0`; record the run shape as exit code, vfs trace line count, resolved API name count, and reached boundary; never judge by the fault address, which is a per-run allocation address; find the candidate whose run shape differs from the rest; confirm reproducibility by re-running once the field narrows to one; and record the outcome in the analysis documents and the walkthrough with a work log.*

## 비범위

- seed 값과 응답 바이트를 이 저장소에 남기는 일. 후보 목록과 매핑 파일은 reSoftlock 저장소에만 둡니다.
- Function `0x0e` 변환 구현.
- 보호 통과 이후 단계의 HLE 구현.

*Out of scope: recording seed values or response bytes in this repository, since candidate lists and map files stay in the reSoftlock repository; implementing the Function `0x0e` transform; and implementing the HLE stages beyond the protection.*

## 완료 조건

- 두 제품 모두 전 후보를 주입 실행하고 결과를 기록합니다.
- 모든 실행의 주입이 완전합니다.
- 판별된 후보가 재실행에서 같은 실행 형태를 보입니다.
- 판별 근거가 fault 주소가 아니라 관찰 가능한 실행 형태입니다.
- 원본 process가 남지 않고 원본 자산이 변경되지 않습니다.
- seed 값과 응답 바이트가 저장소에 들어가지 않습니다.

*Completion requires every candidate injected and recorded for both products, complete injection in every run, the identified candidate reproducing the same run shape, judgement resting on observable run shape rather than the fault address, no original process left running with original assets unchanged, and no seed values or response bytes entering the repository.*
