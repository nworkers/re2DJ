# Windows Injected Runtime Load Probe

## 한국어

entry hardware breakpoint에서 원본 IAT가 loader-resolved 상태임을 확인했다. 다음 probe는 IAT를 바꾸지 않고 x86 `re2dj_windows_injected_runtime.dll`을 original process에 적재한다.

primary guest thread는 entry breakpoint에서 `SuspendThread`로 추가 정지한다. debugger event는 계속 처리해 remote `LoadLibraryW` thread와 DLL loader가 실행될 수 있게 한다. primary thread는 suspend 상태를 유지하므로 guest entry는 실행되지 않는다.

성공 기준은 remote thread가 DLL base를 반환하고, primary thread가 한 번도 resume되지 않은 채 child가 종료되는 것이다.

## English

The entry hardware breakpoint confirmed that the original IAT is loader-resolved. This probe loads an x86 `re2dj_windows_injected_runtime.dll` into the original process without changing the IAT.

The primary guest thread receives an additional `SuspendThread` at the entry breakpoint. Debugger events are continued so that the remote `LoadLibraryW` thread and DLL loader can run. The primary thread remains suspended, so guest entry never executes.

Success means the remote thread returns the DLL base and the child terminates without resuming the primary thread.
