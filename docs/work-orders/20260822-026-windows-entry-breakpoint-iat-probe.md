# Windows Entry Hardware-Breakpoint IAT Probe

## 한국어

1. 첫 debugger breakpoint에서 WOW64 primary thread의 entry execution breakpoint를 설정합니다.
2. entry 직전 single-step event까지 loader를 계속 처리합니다.
3. IAT와 main image base를 읽기 전용으로 검증하고 child를 종료합니다.

## English

1. Set an entry execution breakpoint on the WOW64 primary thread at the first debugger breakpoint.
2. Continue loader events until the single-step event immediately before entry.
3. Verify IAT and main-image base read-only, then terminate the child.
