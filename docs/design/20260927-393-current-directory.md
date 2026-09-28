# 작업 393 설계 — 현재 디렉터리 / Task 393 design — the current directory

선행: [작업 392 설계](20260927-392-surface-sweep.md)

## 배경 / Background

작업 392 뒤 Linux 실행은 `kernel32!GetCurrentDirectoryA(260, buf)`에서 멈췄다. 같은 지점의 Windows 실행 VFS 기록을 보면, 게임은 장면을 바꿀 때 다음 순서를 따른다.

1. 현재 디렉터리를 저장한다.
2. `SetCurrentDirectoryA("System\Common")`으로 옮겨 자원을 이름만으로 연다.
3. 저장한 경로로 되돌아온다.
4. 이어서 `System\AmuseLogo`로 옮긴다.

Linux 파일 모델(`GuestFiles`)은 guest 루트 `D:\ez2dj`를 현재 디렉터리로 고정하고 있었다.

*After Task 392 a Linux run stopped at `kernel32!GetCurrentDirectoryA(260, buf)`. The Windows run's VFS trace shows the game changing scenes in this order:*

1. *It saves the current directory.*
2. *It moves to `System\Common` with `SetCurrentDirectoryA` and opens resources by bare name.*
3. *It returns to the saved path.*
4. *It then moves to `System\AmuseLogo`.*

*Linux's file model (`GuestFiles`) had the guest root `D:\ez2dj` fixed as the current directory.*

### 측정 / Measurements

32비트 PowerShell에서 P/Invoke로 측정했다(Windows 11).

- **`GetCurrentDirectoryA(n, buf)`**
  - n이 경로 길이보다 크면 경로와 NUL을 쓰고 길이를 돌려준다.
  - n이 길이 이하이면 아무것도 쓰지 않고, NUL을 포함해 필요한 크기를 돌려준다. `(0, NULL)`도 같다.
  - n이 충분한데 buf가 NULL이면 access violation이 난다.
  - last error는 어느 경우든 바뀌지 않는다.
- **`SetCurrentDirectoryA(path)`**
  - 성공하면 TRUE이고 last error는 그대로다.
  - 상대 경로는 현재 디렉터리를 기준으로 푼다. `.`와 `..`가 적용되고, `/`도 구분자로 쓰이며, `\\`는 하나로 합쳐진다.
  - 끝의 `\`는 버린다. 드라이브 루트는 예외로 `C:\`다.
  - 호출자가 쓴 대소문자를 그대로 둔다. 예를 들어 `system\COMMON`은 `...\system\COMMON`이 된다.
  - 오류(모두 FALSE):

    | 경우 | last error |
    | --- | --- |
    | 마지막 요소가 없음 | 2 |
    | 중간 요소가 없음 | 3 |
    | 파일을 가리킴 | 267 |
    | 빈 문자열 | 123 |
    | NULL | 87 |

  - 실패하면 현재 디렉터리는 그대로다.
  - `\`는 드라이브 루트로 간다.

*Measured from 32-bit PowerShell through P/Invoke (Windows 11):*

- ***`GetCurrentDirectoryA(n, buf)`***
  - *When n exceeds the path length, it writes the path and a NUL and returns the length.*
  - *When n is at most the length, it writes nothing and returns the size needed, NUL included. `(0, NULL)` behaves the same.*
  - *When n has room but buf is NULL, it raises an access violation.*
  - *The last error never changes.*
- ***`SetCurrentDirectoryA(path)`***
  - *Success is TRUE and leaves the last error alone.*
  - *A relative path resolves against the current directory. `.` and `..` apply, `/` also separates, and `\\` collapses to one.*
  - *A trailing `\` is dropped, except for the drive root `C:\`.*
  - *The caller's case is kept: `system\COMMON` becomes `...\system\COMMON`.*
  - *Errors (all FALSE):*

    | *Case* | *Last error* |
    | --- | --- |
    | *missing last component* | *2* |
    | *missing earlier component* | *3* |
    | *names a file* | *267* |
    | *empty string* | *123* |
    | *NULL* | *87* |

  - *A failure leaves the current directory where it was.*
  - *`\` goes to the drive root.*

## 결정 / Decisions

1. **`GuestFiles`의 현재 디렉터리.** 처음 값은 guest 루트다. 상대 경로(`Open`)는 루트가 아니라 현재 디렉터리를 기준으로 푼다. 요소는 guest가 쓴 철자 그대로 두고, 찾기는 대소문자를 구분하지 않는다.
   ***`GuestFiles`' current directory.** It starts at the guest root, and relative paths (`Open`) resolve against it rather than the root. Components keep the guest's spelling; lookups ignore case.*
2. **이동 규칙.** `SetCurrentDirectory`가 측정한 오류 순서를 따른다.
   - 경로를 해석할 수 없으면 123이다.
   - 루트 아래 요소를 앞에서부터 확인한다. 없으면 마지막 요소일 때 2, 아니면 3이다. 파일이면 마지막일 때 267이다.
   - 중간 요소가 파일인 경우는 측정하지 않았다. 없는 경로와 같게 3으로 둔다.
   - 존재 여부는 쓰기 overlay의 디렉터리와 CHD 이미지를 함께 본다.

   ***Moving.** `SetCurrentDirectory` follows the measured errors:*
   - *A path that does not parse is 123.*
   - *The components below the root are checked in order: a missing one is 2 when last and 3 otherwise; a file is 267 when last.*
   - *A file in the middle was not measured and is treated like a missing path, 3.*
   - *Existence is checked in both the write overlay's directories and the CHD image.*
3. **모델 밖.** 다음은 불리면 멈춘다.
   - guest 루트 밖(드라이브 루트 `\` 포함)으로 가는 경로. 모델이 제공하지 않는 디렉터리다.
   - 충분한 크기와 NULL 버퍼로 부른 `GetCurrentDirectoryA`. Windows에서는 fault다.

   ***Out of the model.** These stop the run:*
   - *A path leading outside the guest root, the drive root `\` included, which the model does not serve.*
   - *`GetCurrentDirectoryA` with room and a NULL buffer, which faults on Windows.*
4. **표시 경로.** Linux guest는 자기 경로를 `D:\ez2dj\...`로 본다. Windows에서는 host 임시 디렉터리 경로가 보인다. 이 차이는 [작업 225](20260908-225-vfs-guest-root-and-chd-enumeration.md)의 guest 루트 결정을 따른다.
   ***The path shown.** A Linux guest sees its path as `D:\ez2dj\...`, where Windows shows the host temporary directory; this follows the guest root decision of [Task 225](20260908-225-vfs-guest-root-and-chd-enumeration.md).*

## 범위 밖 / Out of scope

- 드라이브별 현재 디렉터리(`C:DATA`), `GetFullPathNameA`. / *Per-drive current directories (`C:DATA`) and `GetFullPathNameA`.*
- `GetFileType`: 이번 실행이 fault로 멈추는 원인이다(CRT `fopen`이 파일 handle 종류를 묻는다). 다음 작업이다. / *`GetFileType`, the cause of the fault the run now stops at (the CRT's `fopen` asks a file handle's type): the next task.*
