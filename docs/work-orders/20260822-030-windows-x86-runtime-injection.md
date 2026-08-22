# Windows x86 child runtime DLL 주입 작업 지시

## 작업 내용

1. 기본 Win32 x86 preset에서 최소 injected runtime DLL을 빌드한다.
2. x86 launcher probe가 entry 정지 뒤 primary thread를 유지한 채 runtime DLL을 remote loader thread로 적재하게 한다.
3. module base, IAT 해석 상태, original entry 미실행을 실제 HDD 입력으로 검증한다.
4. 설계·분석·architecture·TODO·porting plan과 작업 로그를 갱신한다.

## English

1. Build the minimal injected runtime DLL in the primary Win32 x86 preset.
2. Make the x86 launcher probe load the runtime DLL through a remote loader thread while keeping the primary thread stopped after the entry break.
3. Verify the module base, resolved IAT state, and non-execution of original entry with live HDD input.
4. Update design, analysis, architecture, TODO, porting plan, and the work log.
