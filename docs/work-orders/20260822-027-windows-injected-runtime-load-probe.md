# Windows Injected Runtime Load Probe

## 한국어

1. Win32 injected runtime DLL target을 추가합니다.
2. entry breakpoint에서 primary thread를 suspend합니다.
3. remote `LoadLibraryW` thread와 필요한 debug events를 처리합니다.
4. DLL base를 확인한 뒤 primary thread를 resume하지 않고 child를 종료합니다.

## English

1. Add a Win32 injected-runtime DLL target.
2. Suspend the primary thread at the entry breakpoint.
3. Handle the remote `LoadLibraryW` thread and required debug events.
4. Confirm the DLL base, then terminate the child without resuming the primary thread.
