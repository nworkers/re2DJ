# 작업 319: Linux guest-memory subrange protection / Task 319: Linux guest-memory subrange protection

설계: [Linux guest-memory subrange protection](../design/20260918-319-linux-guest-memory-subrange-protection.md)

*Design: [Linux guest-memory subrange protection](../design/20260918-319-linux-guest-memory-subrange-protection.md)*

## 작업 / Work

1. i386 helper allocation registry에 page별 access 상태를 기록합니다.
2. page-aligned allocation subrange protect와 uniform previous-access 검사를 추가합니다.
3. memory transfer가 닿는 모든 page의 access를 검사하게 합니다.
4. x64·x86 probe, architecture, L2 계획과 작업 로그를 갱신합니다.

*1. Record per-page access state in the i386 helper allocation registry.
2. Add page-aligned allocation-subrange protection and uniform prior-access validation.
3. Make memory transfer check access on every touched page.
4. Update x64/x86 probes, architecture, the L2 plan, and the work log.*

## 완료 기준 / Completion criteria

8 KiB allocation의 두 번째 4 KiB만 read-only로 바꾸고 첫 page write는 성공, 두 번째 page write는 실패, restore는 성공해야 합니다. 기존 allocation/free와 x64·x86 production-helper probe도 통과해야 합니다.

*Changing only the second 4 KiB of an 8 KiB allocation to read-only must allow a first-page write, reject a second-page write, and allow restore. Existing allocation/free and x64/x86 production-helper probes must continue to pass.*
