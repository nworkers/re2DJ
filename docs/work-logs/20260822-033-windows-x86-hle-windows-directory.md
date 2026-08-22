# Windows x86 HLE GetWindowsDirectoryA 결과

## 결과

`GetWindowsDirectoryA`는 현재 작업 디렉터리가 아니라 `re2dj.exe`가 있는 디렉터리 아래의 `windows` 절대 경로를 반환하도록 구현했습니다. 이 디렉터리는 re2DJ 지원 DLL·리소스 전용이며 HDD와 overlay와 분리됩니다.

실제 `--hle-windows-directory` 제한 실행에서 HLE debugger output event를 수신했습니다. runtime base는 `0x7c160000`이었고 event 직후 child를 종료했습니다.

검증: Win32 warnings-as-errors build 성공, CTest 1/1 통과, live probe 성공.

## English

`GetWindowsDirectoryA` now returns the absolute `windows` directory beside `re2dj.exe`, not a path based on the current working directory. It is reserved for re2DJ support DLLs and resources and is separate from the HDD and overlay.

The live limited `--hle-windows-directory` run received the expected HLE debugger output event. Runtime base was `0x7c160000` and the child was terminated immediately afterward.

Verification: Win32 warnings-as-errors build succeeded, CTest passed 1/1, and the live probe succeeded.
