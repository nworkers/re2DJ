# ez2dj4th Hardlock HLE 호환 경로 조사 작업 지시

관련 설계: [ez2dj4th Hardlock HLE 호환 경로 조사 설계](../design/20260901-130-ez2dj4th-hardlock-bypass-path.md)

*Related design: [ez2dj4th Hardlock HLE compatibility-path investigation](../design/20260901-130-ez2dj4th-hardlock-bypass-path.md).*

## 범위

1. main 병합 결과와 `v0.0.19` tag를 확인합니다.
2. ez2dj4th의 실제 Hardlock 장치 open, dynamic resolver, IOCTL tracker와 외부 profile config 주입 경계를 확인합니다.
3. 사용자가 제시한 2EZConfig-V2 Hardlock 소스의 라이선스와 high-level protocol 계약을 확인하되 코드를 저장소에 복사하지 않습니다.
4. 원본 실행 파일을 수정하지 않는 `FEnteDev` 장치 backend HLE를 유효한 호환 경로 후보로 평가합니다.
5. 유효한 `0x450` 응답과 Function `0x0e` transform의 증거 부족 여부를 명확히 기록하고, 추측 구현을 하지 않습니다.
6. 설계·분석 문서와 작업 로그를 갱신하고 문서 diff 및 비밀값 비추적 상태를 검증합니다.

*1. Confirm the main merge result and `v0.0.19` tag.
2. Confirm ez2dj4th's actual Hardlock device opens, dynamic resolver, IOCTL tracker, and external profile-configuration injection boundary.
3. Confirm the license and high-level protocol contract of the user-provided 2EZConfig-V2 Hardlock source without copying its code into the repository.
4. Evaluate an original-executable-preserving `FEnteDev` device-backend HLE as the valid compatibility-path candidate.
5. Record clearly whether evidence for a valid `0x450` response and Function `0x0e` transform is missing, and do not implement guesses.
6. Update design, analysis, and work-log documents, then verify documentation diff and secret non-tracking.*

## 완료 조건

- `main`의 merge commit과 local annotated tag가 확인됩니다.
- 장치 HLE 후보가 import/dynamic resolver 경계, profile-scoped external config, protocol state machine으로 설명됩니다.
- 2EZConfig 소스는 GPL-3.0-or-later로 판정되고 구현에 포함되지 않습니다.
- 실제 key, seed, module address, transform block, raw dump가 source·문서·로그·명령행·commit에 들어가지 않습니다.
- valid response를 확인하지 못한 상태를 명시하고, 다음 구현에 필요한 증거와 blocker를 기록합니다.
- `git diff --check`가 통과하고 worktree 상태가 의도한 문서 변경만 포함합니다.

*Completion requires confirmation of the main merge commit and local annotated tag; a description of the device-HLE candidate across the import/dynamic-resolver boundary, profile-scoped external configuration, and protocol state machine; classification of 2EZConfig as GPL-3.0-or-later with no implementation inclusion; absence of real key material, module address, transform blocks, or raw dumps from source, docs, logs, command lines, and commits; an explicit unresolved-response blocker and required next evidence; and a clean `git diff --check` with only intended documentation changes.*
