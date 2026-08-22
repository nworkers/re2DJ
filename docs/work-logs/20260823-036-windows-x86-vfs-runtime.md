# Windows x86 VFS runtime 결과

## 결과

공용 `VfsRoots`는 overlay 파일을 먼저 찾아 읽도록 수정했고, `VfsFileTable` 단위 테스트로 overlay read, write, seek, close를 검증했습니다. Windows x86 launcher의 `--hle-vfs`는 runtime에 HDD root와 CWD 기준 `overlays/<target-id>`를 전달하고, 원본 `KERNEL32.dll` IAT의 일곱 file API를 wrapper로 교체합니다.

runtime의 write open은 원본 파일을 직접 열지 않습니다. 기존 파일을 수정할 때 overlay 사본이 없으면 원본을 overlay로 먼저 복사합니다. synthetic runtime probe는 원본을 읽고, `OPEN_EXISTING` write가 overlay 사본만 바꾸며 원본은 그대로인 것을 확인합니다.

사용자가 제공한 `roms/ez2dj1stse` HDD로 `--hle-vfs` 제한 실행을 수행해 원본 `ez2dj1.exe` entry의 `CreateFileA` wrapper debugger event를 한 번 수신했습니다. x86 `__stdcall` runtime export가 `_Re2djVfsCreateFileA@28`처럼 장식된 이름으로 PE export table에 기록되는 문제를 확인하고 launcher 조회 이름을 수정했습니다. 이후 재시도에서는 10초 관찰 창 안에 호출이 재현되지 않았으므로, 원본 read/write/close와 overlay 결과의 반복 검증은 TODO에 남겼습니다. device path (`\\.\\`)도 path trace 전까지는 매핑하지 않고 실패합니다.

## English

The shared `VfsRoots` now checks the overlay first for reads, and `VfsFileTable` unit tests cover overlay reads, writes, seeks, and close. The Windows x86 launcher's `--hle-vfs` configures the runtime with the HDD root and CWD-based `overlays/<target-id>`, then replaces the seven file-API slots in the original `KERNEL32.dll` IAT.

The runtime never opens an original file for a write. When an existing file is modified and no overlay copy exists, it copies the original into the overlay first. The synthetic runtime probe reads an original file and confirms that an `OPEN_EXISTING` write changes only the overlay copy.

Using the user-supplied `roms/ez2dj1stse` HDD, one limited `--hle-vfs` run received the `CreateFileA` wrapper debugger event from the original `ez2dj1.exe` entry. This also exposed that x86 `__stdcall` runtime exports appear in the PE export table with decorated names such as `_Re2djVfsCreateFileA@28`, so the launcher lookup was corrected. Later retries did not reproduce the call within the ten-second observation window, so repeated verification of original read/write/close and overlay results remains in TODO. Until a path trace exists, device paths such as `\\.\\` are not mapped and fail.
