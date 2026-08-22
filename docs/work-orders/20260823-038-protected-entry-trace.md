# Protected entry debugger trace 작업 지시

1. `--trace`가 loader·protection 단계의 상세 debugger event를 출력하게 한다.
2. software entry breakpoint가 pre-entry에 덮어써지는지 기록한다.
3. IAT wrapper로 protected image의 `ExitProcess` caller를 종료 동작 변경 없이 관찰한다.
4. 실제 `roms/ez2dj1stse/ez2dj.exe` pre-entry 실행을 기록한다.
5. 확인됨·추정·미확정을 구분해 분석 문서와 TODO를 갱신한다.
6. 범위에 맞는 Windows x86 build와 test를 실행한다.

## English

1. Make `--trace` report detailed debugger events from the loader and protection stage.
2. Record whether the software entry breakpoint is overwritten before entry.
3. Observe the protected image's `ExitProcess` caller through an IAT wrapper without changing termination behavior.
4. Record a pre-entry run of live `roms/ez2dj1stse/ez2dj.exe`.
5. Update analysis and TODO with confirmed, inferred, and unresolved states distinguished.
6. Run the appropriate Windows x86 build and tests.
