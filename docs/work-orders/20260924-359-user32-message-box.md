# 작업 359 작업 지시서 — `user32!MessageBoxA` facade export / Task 359 work order — `user32!MessageBoxA` facade export

설계: [20260924-359-user32-message-box.md](../design/20260924-359-user32-message-box.md)

## 단계 / Steps

1. `user32` facade에 `MessageBoxA`와 기본 버튼 규칙을 추가하고 단위 테스트를 갱신한다.
2. `OriginalApiCall`에 두 번째 문자열을 추가하고, continuation이 facade export의 문자열 인자를 두 개까지 기록하게 한다. CLI는 비ASCII byte를 `\xNN`으로 출력한다.
3. Linux x64·x86과 Windows x86을 빌드하고 CTest, probe를 실행한다. 실제 4th CHD로 두 폭의 새 경계를 확인한다.
4. 분석, `ARCHITECTURE.md`, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) add `MessageBoxA` with the default-button rule to the `user32` facade and update the unit tests; (2) add a second string to `OriginalApiCall`, have the continuation record up to two string arguments of a facade export, and print non-ASCII bytes as `\xNN` in the CLI; (3) build Linux x64/x86 and Windows x86, run CTest and the probes, and check the new boundary on the real 4th CHD on both widths; (4) update the analysis, `ARCHITECTURE.md`, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* 단위 테스트가 통과한다.
* 실제 4th CHD의 `#0019`가 두 폭에서 처리되고, 문구·제목과 다음 경계가 같게 기록된다.

*Completion: the unit tests pass, and on the real 4th CHD `#0019` is handled on both widths with the same text, caption, and next boundary recorded.*
