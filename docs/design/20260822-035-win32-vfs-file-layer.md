# Win32 VFS file layer

## 설계

Stage 5의 공용 최소 파일 계층은 `VfsFileTable`로 구현한다. Windows x86 runtime은 원본 프로세스와 같은 bitness이므로, IAT의 `CreateFileA`, `ReadFile`, `WriteFile`, `SetFilePointer`, `GetFileSize`, `CloseHandle`, `GetFileType`를 wrapper로 바꾸고 host `HANDLE`을 그대로 사용한다. 이 선택은 runtime 경계에 한정되며 공용 코어의 guest handle table과 혼동하지 않는다.

`C:\\windows`와 `D:\\ez2dj`는 `VfsRoots`로 분기한다. 읽기는 CWD 기준 `overlays/<target-id>`를 먼저 검색하고, 없으면 support/HDD를 검색한다. 쓰기·truncate·append는 항상 overlay에만 기록하며 부모 디렉터리를 생성한다. `OPEN_EXISTING` 또는 `OPEN_ALWAYS` 쓰기에서 overlay 사본이 없고 원본이 존재하면 먼저 복사한다. 원본 HDD와 support directory는 절대 쓰지 않는다.

## English

The Stage 5 shared minimum file layer is implemented through `VfsFileTable`. Because the Windows x86 runtime has the same bitness as the original process, it replaces the IAT entries for `CreateFileA`, `ReadFile`, `WriteFile`, `SetFilePointer`, `GetFileSize`, `CloseHandle`, and `GetFileType` with wrappers that use host `HANDLE` values directly. This runtime-only choice does not replace the platform-neutral guest handle table.

`C:\\windows` and `D:\\ez2dj` branch through `VfsRoots`. Reads search the CWD-based `overlays/<target-id>` first and fall back to support/HDD. Writes, truncation, and append always target the overlay and create parent directories. If an `OPEN_EXISTING` or `OPEN_ALWAYS` write has no overlay copy but has an original file, it copies that original first. The original HDD and support directory are never written.
