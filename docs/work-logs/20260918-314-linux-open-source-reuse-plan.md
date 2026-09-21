# 작업 로그 314 — Linux 외부 오픈소스 재사용 계획 / Work log 314 — Linux open-source reuse plan

사용자가 승인한 외부 구현 재사용 방침을 [Linux 설계](../design/20260918-307-linux-x86-x64-wsl.md)와 [구현 계획](../work-orders/20260918-307-linux-x86-x64-wsl.md)에 반영했다. SDL backend 공용화, L4 callback 전 dyncall/dyncallback 평가, Linux raw-I/O decoder 확장 전 Zydis 평가, 실제 CP949 변환 요구에 따른 ICU 검토를 명시했다.

*Recorded the user-approved reuse policy in the [Linux design](../design/20260918-307-linux-x86-x64-wsl.md) and [implementation plan](../work-orders/20260918-307-linux-x86-x64-wsl.md): share SDL backends, evaluate dyncall/dyncallback before L4 callbacks, evaluate Zydis before Linux raw-I/O decoder expansion, and consider ICU for an actual CP949 conversion requirement.*

각 후보의 책임 경계, 적용 시점, x86/x64와 공통 i386 helper의 검증 기준, 고정 revision·라이선스·전이 의존성 검토, 부적합 시 최소 자체 구현으로 돌아가는 기준을 추가했다. 원본 실행·Win32 HLE·보드·overlay 정책은 프로젝트 책임으로 유지한다. 현재 진행 상태를 초기 조사와 구분하고 작업 310의 Web 제외 정책이 우선함을 명시했다.

*Added responsibility boundaries, timing, acceptance criteria for both hosts and the shared i386 helper, revision/license/dependency checks, and fallback to minimum custom implementation when unsuitable. Original execution, Win32 HLE, board, and overlay policies remain project responsibilities. Distinguished current progress from initial observations and made Task 310's Web exclusion authoritative.*

문서 diff와 상대 링크, 한국어/영어 대응 및 `git diff --check`를 확인했다. 코드·build 설정·의존성을 변경하지 않아 build/test는 실행하지 않았다. 이 기록은 라이브러리의 프로젝트 통합이나 런타임 검증 완료를 뜻하지 않는다.

*Checked document diffs, relative links, Korean/English coverage, and `git diff --check`. No build or tests were run because code, build settings, and dependencies were unchanged. This record does not claim completed library integration or runtime validation.*
