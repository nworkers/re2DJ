# Windows x86 첫 import handoff 작업 지시

## 작업 내용

1. runtime DLL에 원래 API 주소 설정 export와 log-and-forward thunk를 추가한다.
2. launcher가 runtime export RVA와 `GetCommandLineA` IAT slot을 찾아 patch한다.
3. entry를 제한적으로 재개해 debugger output event로 첫 handoff를 확인하고 즉시 child를 종료한다.
4. build, CTest, 실제 HDD 결과를 문서화한다.

## English

1. Add an original-target configuration export and a log-and-forward thunk to the runtime DLL.
2. Make the launcher find the runtime export RVA and `GetCommandLineA` IAT slot, then patch it.
3. Resume entry for a limited interval, confirm the first handoff through a debugger output event, and terminate the child immediately.
4. Document build, CTest, and live-HDD results.
