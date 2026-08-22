# Windows x86 첫 import handoff

## 설계

runtime DLL은 `GetCommandLineA` IAT slot의 원래 target을 보관하고, launcher는 그 slot을 runtime의 x86 thunk로 교체한다. thunk는 debugger output event로 호출을 알린 뒤 레지스터와 stack을 보존한 채 원래 target으로 tail-jump한다. 따라서 관찰 단계는 API 결과와 stdcall stack cleanup을 바꾸지 않는다.

`GetCommandLineA`는 확인된 `ez2dj1.exe` import table의 첫 kernel32 import이며, 이 단계의 대상 선택은 정적 import 순서에 근거한다. 실제 entry 이후 호출 여부는 runtime output event로만 확인한다.

## English

The runtime DLL retains the original target from the `GetCommandLineA` IAT slot, and the launcher replaces that slot with an x86 runtime thunk. The thunk reports the call through a debugger output event, then preserves registers and stack while tail-jumping to the original target. The observation step therefore does not alter API results or stdcall cleanup.

`GetCommandLineA` is the first kernel32 import in the confirmed `ez2dj1.exe` import table, so the target choice is based on static import order. Whether it is actually called after entry is confirmed only through the runtime output event.
