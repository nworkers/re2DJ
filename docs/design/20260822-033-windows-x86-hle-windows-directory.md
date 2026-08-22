# Windows x86 HLE GetWindowsDirectoryA

## 설계

사용자 결정에 따라 `GetWindowsDirectoryA`는 host의 실제 Windows directory를 반환하지 않는다. **`re2dj.exe`가 있는 디렉터리** 아래 `windows` 디렉터리의 절대 경로를 반환한다. 현재 작업 디렉터리(CWD)는 이 경로를 바꾸지 않는다. 이 디렉터리는 re2DJ가 제공하는 지원 리소스와 DLL의 관리 위치이며, 원본 HDD와 `overlays/<target-id>`와 분리된다.

runtime HLE 함수는 caller buffer와 size를 존중해 NUL-terminated ANSI 경로를 복사하고, 필요 buffer 크기를 제외한 경로 길이를 반환한다. target resource의 경로가 아니라 runtime support directory라는 점을 유지한다.

## English

By user decision, `GetWindowsDirectoryA` does not return the host's actual Windows directory. It returns the absolute path of the `windows` directory beside **`re2dj.exe`**. The current working directory does not affect this path. This directory is the managed location for re2DJ support resources and DLLs, separate from the original HDD and `overlays/<target-id>`.

The runtime HLE function honors the caller buffer and size, copies a NUL-terminated ANSI path, and returns the path length excluding the terminating NUL. It remains a runtime-support directory rather than a target-resource path.
