# Win32 파일 삭제와 이름의 대소문자 / Win32 file deletion and name case

## `DeleteFileA`

`BOOL DeleteFileA(LPCSTR lpFileName)`은 파일 하나를 지운다. 성공하면 0이 아닌 값, 실패하면 0을 돌려주고 `GetLastError`로 이유를 알린다([Microsoft 문서](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilea)).

| 경우 / Case | 오류 / Error |
| --- | --- |
| 파일이 없음 / the file does not exist | `ERROR_FILE_NOT_FOUND` (2) |
| 경로의 디렉터리가 없음 / a directory on the path does not exist | `ERROR_PATH_NOT_FOUND` (3) |
| 디렉터리이거나 읽기 전용 / a directory or read-only | `ERROR_ACCESS_DENIED` (5) |
| 다른 핸들이 `FILE_SHARE_DELETE` 없이 열어 둠 / open elsewhere without `FILE_SHARE_DELETE` | `ERROR_SHARING_VIOLATION` (32) |

오류 코드 값은 [System Error Codes (0-499)](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-)를 따른다. 성공했을 때 last error를 어떻게 두는지는 문서에 없다.

*`DeleteFileA` deletes one file, returning non-zero on success and zero on failure with the reason in `GetLastError` ([Microsoft documentation](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilea)), with the errors tabulated above (values from [System Error Codes (0-499)](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-)). The documentation does not say what happens to the last error on success.*

## 이름의 대소문자 / Name case

NTFS와 FAT는 Win32에서 이름의 대소문자를 보존하되 비교에서는 구분하지 않는다. 그래서 `EZ2DJ1st\a.ini`와 `EZ2DJ1ST\A.INI`는 같은 파일이다([Microsoft 문서: Case sensitivity](https://learn.microsoft.com/en-us/windows/wsl/case-sensitivity)). Linux의 일반 파일 시스템은 구분하므로, 게스트 경로를 host 경로로 옮길 때는 구성요소마다 대소문자 없이 기존 항목을 찾아야 같은 파일이 된다.

*NTFS and FAT under Win32 preserve name case but compare without it, so `EZ2DJ1st\a.ini` and `EZ2DJ1ST\A.INI` are one file ([Microsoft: Case sensitivity](https://learn.microsoft.com/en-us/windows/wsl/case-sensitivity)). Ordinary Linux file systems tell case apart, so a guest path mapped to a host path must find existing entries without case, component by component, to reach the same file.*

## 읽기 전용 이미지 위의 삭제 / Deleting over a read-only image

읽기 전용 아래층 위에 쓰기 층을 두는 overlay 방식에서는 아래층 파일을 실제로 지울 수 없다. 그래서 "지웠다"는 표시(whiteout)를 쓰기 층에 남기고, 조회할 때 아래층의 그 파일을 없는 것으로 본다. Linux overlayfs는 같은 이름의 character device 0/0을 whiteout으로 쓴다([커널 문서](https://docs.kernel.org/filesystems/overlayfs.html#whiteouts-and-opaque-directories)). re2DJ는 overlay 루트의 목록 파일을 쓴다([작업 437 설계](../design/20261003-437-remember-1st-handoff-files.md)).

*With an overlay, a writable layer over a read-only lower one, a lower file cannot really be deleted; a "deleted" mark (a whiteout) is left in the upper layer, and lookups treat the lower file as missing. Linux overlayfs uses a character device 0/0 of the same name ([kernel documentation](https://docs.kernel.org/filesystems/overlayfs.html#whiteouts-and-opaque-directories)); re2DJ uses a list file at the overlay root ([task 437 design](../design/20261003-437-remember-1st-handoff-files.md)).*
