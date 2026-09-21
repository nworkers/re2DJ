# 작업 308 — WSL Linux 기준선 재현 / Task 308 — WSL Linux Baseline

설계는 [작업 307](../design/20260918-307-linux-x86-x64-wsl.md)의 L0 범위를 따른다. 사용자의 계속 진행 요청에 따라 기존 구현을 현재 commit에서 재빌드하고 검증한다.

*Follow L0 of [design 307](../design/20260918-307-linux-x86-x64-wsl.md). The user's continuation request authorizes rebuilding and validating the existing implementation at the current commit.*

1. WSL·compiler·multilib·GUI 환경과 기존 dependency source cache를 확인한다.
2. Linux filesystem의 별도 임시 build tree에서 x64 제품 전체와 i386 helper를 warnings-as-errors로 빌드한다. 기존 cache를 사용하면 고정 revision과 일치하는지 확인한다.
3. x64 CTest, ELF 폭, synthetic helper 결과 51 및 SIGILL context를 검증한다. 가능한 경우 기존 OpenGL probe를 실행한다.
4. 실패는 재현 명령과 함께 기록하고 L1 변경 목록을 확정한다. 기준선을 막는 단순 compiler 경고는 [빌드 복구 설계](../design/20260918-308-linux-wsl-baseline.md)에 따라 수정하고 재빌드한다. 실제 게임 실행·새 HLE 구현·x86 제품 지원 구현은 이 작업 범위 밖이다.
5. 결과 로그를 남기고 커밋한다. 원본 자산은 사용하지 않는다.

*Inventory WSL, compilers, multilib, GUI, and dependency caches. Build the complete x64 product and i386 helper with warnings-as-errors in separate temporary Linux-filesystem trees, verifying cached dependency revisions if used. Run x64 CTest, inspect ELF classes, and verify synthetic result 51 and SIGILL context; run the existing OpenGL probe if possible. Record reproducible failures and specify L1 changes. Actual games, new HLE implementation, and x86 product support are outside this task. Commit the result log without using original assets.*

*Repair simple compiler warnings blocking the baseline according to the [repair design](../design/20260918-308-linux-wsl-baseline.md), then rebuild.*
