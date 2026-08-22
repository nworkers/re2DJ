# Windows x86 HLE GetCommandLineA

## 설계

사용자 결정에 따라 첫 실제 HLE API인 `GetCommandLineA`는 원본 process command line을 forwarding하지 않고 **원본 실행 파일명만** 반환한다. launcher는 선택된 target executable의 basename을 runtime DLL의 exported ANSI buffer에 기록하고, IAT slot을 HLE thunk로 교체한다.

thunk는 buffer 주소를 EAX로 반환하고 인자가 없는 Win32 API 규약대로 `ret`한다. 반환 포인터는 runtime DLL의 process-lifetime storage이므로 caller가 해제하면 안 된다. debugger output event는 실제 HLE 호출 증거로 유지한다.

## English

By user decision, the first real HLE API, `GetCommandLineA`, does not forward the original process command line. It returns **only the original executable filename**. The launcher writes the selected target executable basename into an exported ANSI buffer in the runtime DLL and replaces the IAT slot with the HLE thunk.

The thunk returns the buffer address in EAX and uses `ret`, matching the no-argument Win32 API convention. The returned pointer is process-lifetime runtime-DLL storage and must not be freed by callers. A debugger output event remains evidence that the real HLE call occurred.
