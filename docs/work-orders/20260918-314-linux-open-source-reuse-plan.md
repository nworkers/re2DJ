# 작업 314 — Linux 외부 오픈소스 재사용 계획 / Task 314 — Linux open-source reuse plan

사용자 승인에 따라 [Linux 설계](../design/20260918-307-linux-x86-x64-wsl.md)의 재사용 방침을 먼저 갱신하고 [L0~L7 계획](20260918-307-linux-x86-x64-wsl.md)에 단계별 선행 검증을 추가한다. SDL 재사용을 우선하며 dyncall·Zydis는 구현 전 비교 검증, ICU는 실제 변환 요구가 생겼을 때 검토한다. 문서만 변경하고 의존성 설치나 코드 구현은 후속 작업으로 남긴다.

*Following user approval, first update the reuse policy in the [Linux design](../design/20260918-307-linux-x86-x64-wsl.md), then add phase-specific prerequisites to the [L0–L7 plan](20260918-307-linux-x86-x64-wsl.md). Prioritize SDL reuse, evaluate dyncall and Zydis before relevant implementation, and consider ICU when conversion is required. This task changes documentation only; dependency installation and implementation remain follow-up work.*

검증은 변경 diff, 문서 상대 링크, 한국어/영어 대응, 후보와 확정 의존성 구분 및 `git diff --check`로 수행한다. 결과를 대응 작업 로그에 기록하고 문서 변경을 커밋한다.

*Verify the diff, relative document links, Korean/English coverage, candidate-versus-adopted status, and `git diff --check`. Record results in the corresponding work log and commit the documentation changes.*
