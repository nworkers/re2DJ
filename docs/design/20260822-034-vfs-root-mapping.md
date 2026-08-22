# VFS mount root mapping

## 설계

공용 `VfsRoots`는 두 guest mount를 관리한다. `C:\\windows`는 `re2dj.exe` 옆 support directory에 매핑하고, `D:\\ez2dj`는 사용자 HDD에 매핑한다. 모든 write는 현재 작업 디렉터리의 `overlays/<target-id>` 아래 동일한 guest 상대 경로로 보낸다. 읽기는 overlay 우선이며, overlay miss일 때만 HDD 또는 support directory를 조회한다.

## English

The shared `VfsRoots` manages two guest mounts. `C:\\windows` maps beside `re2dj.exe` to the support directory, while `D:\\ez2dj` maps to the user HDD. Every write goes to the current working directory's `overlays/<target-id>` using the same guest-relative path. Reads consult the overlay first and fall back to the HDD or support directory only on an overlay miss.
