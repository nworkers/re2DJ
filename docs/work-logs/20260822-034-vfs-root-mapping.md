# VFS mount root mapping 결과

## 결과

공용 `VfsRoots`와 `ResolveVfsPath`를 추가했습니다. 지원 경로는 `C:\\windows`, HDD 경로는 `D:\\ez2dj`로 분기하며, write는 두 경로 모두 CWD 기준 overlay로 보냅니다.

Win32 warnings-as-errors build와 CTest 1/1을 통과했습니다. 아직 Win32 handle table과 `CreateFileA` thunk는 다음 작업입니다.

## English

The shared `VfsRoots` and `ResolveVfsPath` were added. Support paths branch at `C:\\windows`, HDD paths at `D:\\ez2dj`, and writes for both mounts go to the CWD-based overlay.

The Win32 warnings-as-errors build and CTest 1/1 passed. The Win32 handle table and `CreateFileA` thunk are the next task.
