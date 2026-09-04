# 20260904-177 VFS 게스트 현재 디렉터리 설계
# 20260904-177 VFS Guest Working Directory Design

## 1. 배경 및 목적 (Background & Objectives)

Task 175에서 게스트의 자원 적재 실패 원인을 확정했다. 게스트는 자기 현재 디렉터리를 바꾼 뒤 `2PLAYERInsertCoin.str` 같은 상대 이름으로 파일을 연다. re2DJ의 VFS는 상대 이름을 언제나 HDD 루트에 붙이므로 `EZ2DJ/2PLAYERInsertCoin.str`을 찾고, 실제 파일은 `EZ2DJ/SYSTEM/Common/`에 있다.

Task 176이 FAT32 긴 이름 결함을 고쳐 그 파일이 이미지에서 보이게 되었지만, 경로 해석은 그대로다. 이 작업은 VFS가 게스트의 현재 디렉터리를 추적하게 한다.

The guest changes its working directory and then opens resources by bare name, while the VFS joins every relative name to the HDD root. Task 176 made the files visible in the image; this task makes the VFS resolve names the way the guest means them.

---

## 2. 관측된 사실 (Observed Facts)

- 게스트는 `SetCurrentDirectoryA`와 `GetCurrentDirectoryA`를 동적으로 해석해 쓴다.
- re2DJ는 두 API를 대체하지 않으므로 호스트의 실제 API가 실행되고, 그 결과는 VFS 매핑에 반영되지 않는다.
- 성공하는 열기는 모두 게스트가 만든 절대 경로다. 즉 게스트는 우리가 돌려준 절대 경로를 그대로 다시 넘길 수 있다.
- 실패하는 열기 34건은 모두 디렉터리 없는 상대 이름이다.

The guest resolves and uses both directory APIs, re2DJ replaces neither, every successful open is an absolute path the guest built from what we returned, and every failure is a bare relative name.

---

## 3. 설계 (Design)

```mermaid
flowchart TD
    A["SetCurrentDirectoryA(request)"] --> B[요청을 게스트 상대 경로로 해석]
    B --> C{디렉터리가 존재하는가}
    C -- 아니오 --> D[FALSE, ERROR_PATH_NOT_FOUND]
    C -- 예 --> E[현재 디렉터리 상태 갱신]
    F["CreateFileA(\"2PLAYERInsertCoin.str\")"] --> G[상대 이름을 현재 디렉터리에 결합]
    E -.-> G
    G --> H["EZ2DJ/SYSTEM/Common/2PLAYERInsertCoin.str"]
    H --> I[overlay / native / CHD 조회]
    J["GetCurrentDirectoryA"] --> K["매핑된 native 절대 경로 반환"]
    E -.-> K
```

### 3.1 상태 (State)

주입 런타임이 게스트의 논리 현재 디렉터리를 HDD 루트 기준 구성요소 목록으로 들고 있는다. 초기값은 비어 있고, 그것이 게스트가 보는 루트, 즉 `EZ2DJ` 디렉터리다.

호스트 프로세스의 실제 작업 디렉터리는 바꾸지 않는다. 매핑이 모든 일을 하며, 호스트 디렉터리를 함께 움직이면 아직 materialize되지 않은 경로에서 실패하거나 다른 호스트 API의 동작을 바꾼다.

### 3.2 경로 해석 (Path Resolution)

경로 파싱과 결합은 이미 있는 `include/re2dj/storage/guest_path.h`를 쓴다. 이 단위는 Win32 경로 다섯 형태를 구분하고, `.`과 `..`을 접고, 루트를 벗어나는 경로를 거부하며, 이미 단위 시험이 있다. 새 파서를 쓰지 않는다.

| 요청 형태 | 해석 |
| - | - |
| HDD 루트로 시작하는 native 절대 경로 | 지금처럼 루트 기준 접미사 |
| `D:\ez2dj\...` | 지금처럼 루트 기준 접미사 |
| `\DATA\X` (root-relative) | 루트 기준 |
| `DATA\X`, `X` (relative) | **현재 디렉터리 기준** |
| `C:\windows\...` | 지금처럼 지원 디렉터리 |

결합 결과는 한 곳에서 만들어 native 경로와 CHD 상대 경로가 같은 해석을 공유하게 한다. 지금은 `MapVfsPath`와 `ChdRelativePath`가 각각 상대 이름을 다루므로, 둘 다 이 해석을 거치게 한다.

### 3.3 `SetCurrentDirectoryA`

요청을 해석한 뒤 대상이 디렉터리로 존재할 때만 상태를 갱신한다. 존재 확인은 overlay와 native 트리를 먼저 보고, 없으면 CHD를 본다. CHD가 원본이고 native 트리는 필요할 때만 채워지는 캐시이기 때문이다.

실패하면 상태를 바꾸지 않고 `FALSE`와 `ERROR_PATH_NOT_FOUND`를 돌려준다. 원본 게스트도 이 API의 실패를 그대로 받는다.

### 3.4 `GetCurrentDirectoryA`

매핑된 native 절대 경로를 돌려준다. 게스트가 이미 그 형태를 받아 다시 넘기고 있고 그 경로가 정상 동작하기 때문이다. 버퍼가 모자라면 Win32 규약대로 필요한 길이(종료 문자 포함)를 반환한다.

---

## 4. 경계 (Boundaries)

- 대소문자는 지금처럼 무시한다. FAT와 Win32 모두 그렇게 동작한다.
- `..`이 루트를 벗어나면 요청을 거부한다. 원본 자산 디렉터리 밖으로 나가는 경로를 만들지 않는다.
- 디렉터리 상태는 프로세스 하나에 하나다. 게스트가 여러 스레드에서 바꾸면 마지막 호출이 이긴다. 원본도 Win32 규약상 프로세스 단위다.

Case is ignored as before, `..` above the root is rejected so no path escapes the asset directory, and the directory is process-wide exactly as Win32 defines it.

---

## 5. 검증 (Verification)

- 단위 시험: 경로 결합 규칙은 `guest_path` 단위 시험이 이미 덮는다. 이번 작업에서 추가로 필요한 것은 런타임 상태 전이인데, 주입 런타임은 Windows 프로세스 안에서만 의미가 있으므로 실행 진단으로 확인한다.
- 실행 진단: 진입 추적을 다시 실행해 `.vfs.log`에서 `2PLAYERInsertCoin.str` 요청이 `EZ2DJ/SYSTEM/Common`으로 매핑되어 성공하는지, 그리고 접근 위반이 사라지는지 본다.

---

## 6. 비목표 (Non-Goals)

- 호스트 프로세스 작업 디렉터리 변경.
- `SetCurrentDirectoryW` 등 wide API. 게스트는 ANSI만 쓴다.
- 드라이브별 현재 디렉터리(`C:DATA` 형태)의 완전한 구현. 파서는 이 형태를 구분하지만 이 프로젝트는 게스트 드라이브를 하나만 마운트한다.
- FAT32 쓰기 지원.

No host directory change, no wide APIs, no per-drive current directories, and no FAT32 write support.
