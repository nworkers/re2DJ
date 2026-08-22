# Windows x86 HLE GetCommandLineA 작업 지시

## 작업 내용

1. runtime DLL에 command-line buffer와 HLE `GetCommandLineA` thunk를 구현한다.
2. launcher가 target basename을 runtime buffer에 쓰고 HLE thunk로 IAT를 patch한다.
3. 실제 entry 재개에서 HLE event와 반환 pointer의 문자열을 검증한다.
4. 결과를 analysis, architecture, TODO, porting plan, work log에 기록한다.

## English

1. Implement a command-line buffer and HLE `GetCommandLineA` thunk in the runtime DLL.
2. Make the launcher write the target basename into that buffer and patch the IAT to the HLE thunk.
3. Verify the HLE event and returned pointer string during a live entry resume.
4. Record the result in analysis, architecture, TODO, porting plan, and the work log.
