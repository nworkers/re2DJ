# VFS 게스트 루트 접두사와 CHD 열거 현재 디렉터리 설계

## 한국어

### 목적

[ez2dj1stse Hardlock 후보 판별](20260908-224-ez2dj1stse-hardlock-candidate-judgement.md)에서 복호화된 게스트가 실행에 성공한 뒤, 작업 디렉터리 관련 호출 두 건이 실패하는 것이 관측되었습니다. 두 실패의 원인을 각각 고칩니다.

관측된 줄은 다음과 같습니다.

```text
current-directory:stage=set:request=c:\ez2dj:resolved=ez2dj:success=0
current-directory:stage=set:request=System\Title:resolved=System/Title:success=1
find-first:name=*.*:chd_dir=EZ2DJ:pattern=*.*:matches=10:handle=0xfcce0001
current-directory:stage=set:request=Songs:resolved=System/Title/Songs:success=0
```

### 결함 1 — 게스트 루트 접두사가 `D:\ez2dj`로 고정되어 있다 — 확인됨

`StripGuestRoot`는 매핑된 HDD 루트와 문자열 `"D:\\ez2dj"` 두 가지만 루트로 인정합니다.

1st SE CHD 빌드의 게스트 부팅 경로는 `C:\ez2dj`이고([파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)), 실제로 `SetCurrentDirectory("c:\ez2dj")`를 호출합니다. 이 이름은 루트로 인정되지 않으므로 drive-absolute 경로로 파싱되고, `request.kind`가 relative로 덮어써진 뒤 빈 base와 결합되어 `ez2dj` 한 구성요소로 줄어듭니다. VFS 루트가 이미 그 `ez2dj` 디렉터리이므로 `GuestDirectoryExists("ez2dj")`는 거짓이고 호출은 실패합니다. 관측된 `resolved=ez2dj:success=0`과 정확히 일치합니다.

드라이브 문자는 덤프의 성질이지 제품의 성질이 아닙니다. 이미 `TargetProfile`이 `guest_drive_letter`와 `guest_directory`를 들고 있으므로, 런타임이 그 값을 받아야 합니다.

### 결함 2 — CHD 열거가 게스트 현재 디렉터리를 잃는다 — 확인됨

`Re2djVfsFindFirstFileA`의 CHD 분기는 와일드카드를 포함한 이름 전체를 `ResolveGuestRelativePath`에 넘깁니다. `ParseGuestPath`는 `*`와 `?`를 파일명에 쓸 수 없는 문자로 거부하므로(`IsForbiddenComponentCharacter`) 해석이 실패하고, else 분기가 원본 이름을 그대로 쪼갭니다. 디렉터리 부분이 비어 `chd_dir`이 CHD 루트 `EZ2DJ`가 되어 현재 디렉터리가 사라집니다.

같은 함수의 native 분기에는 이 결함이 없습니다. `MapVfsSearchPath`가 **패턴을 먼저 떼어 내고** 디렉터리만 매핑하며, 디렉터리가 없으면 `"."`를 씁니다. 그래서 CHD 없이 도는 `re2dj_windows_vfs_runtime_probe`의 열거 시험은 통과해 왔고, 결함은 CHD 경로에만 남아 있었습니다.

즉 파서가 와일드카드를 거부하는 것은 옳고, 잘못은 해석 전에 패턴을 분리하지 않은 호출 쪽에 있습니다.

```mermaid
flowchart TD
    A["FindFirstFileA(\"*.*\")"] --> B{CHD 분기 / CHD branch}
    B --> C["ResolveGuestRelativePath(\"*.*\")"]
    C --> D["ParseGuestPath가 '*' 거부<br/>ParseGuestPath rejects '*'"]
    D --> E["fallback: 원본 이름 분해<br/>split the raw name"]
    E --> F["chd_dir = EZ2DJ (루트) / (root)"]
    A --> G{native 분기 / native branch}
    G --> H["MapVfsSearchPath: 패턴 먼저 분리<br/>split the pattern first"]
    H --> I["디렉터리만 매핑, 없으면 '.'<br/>map the directory only, '.' when absent"]
    I --> J["현재 디렉터리 유지 / current directory kept"]
```

### 변경 정책

| 항목 | 변경 |
| --- | --- |
| `g_re2dj_vfs_guest_root` | 런타임에 export 추가. 게스트가 자기 루트를 부르는 이름 하나를 담는다 |
| launcher | 프로파일의 `guest_drive_letter` + `guest_directory`로 채운다. 프로파일에 없으면 기존 값 `D:\ez2dj`를 쓴다 |
| `StripGuestRoot` | 고정 문자열 대신 이 값 하나를 본다 |
| `Re2djVfsFindFirstFileA` CHD 분기 | `MapVfsSearchPath`와 같이 패턴을 먼저 분리하고 디렉터리만 해석한다. 디렉터리가 없으면 `"."` |

launcher는 주 프로세스와 bootstrap child 두 주입 지점 모두에 같은 값을 씁니다.

프로파일에 게스트 경로가 없는 대상(1st, 2nd, 3rd, 4th, 5th, 6th)은 launcher가 `D:\ez2dj`를 쓰므로 동작이 그대로입니다. 1st SE만 `C:\ez2dj`로 바뀝니다.

### 고치지 않는 것

`SetCurrentDirectory("Songs")`가 `System/Title/Songs`로 해석되는 것은 **Win32 의미대로 옳습니다.** 상대 경로는 현재 디렉터리 기준입니다. 이 호출이 실패한 것은 그 앞의 열거가 CHD 루트를 훑어 게스트가 루트의 `Songs` 항목을 본 결과로 **추정**되며, 결함 2를 고치면 게스트가 보는 목록이 달라집니다. 상대 경로 의미는 바꾸지 않고, 수정 후 도달 경계를 다시 관측해 판단합니다.

`ChdRelativePath`와 `GuestDirectoryExists`가 CHD 안 게임 디렉터리 이름을 `"EZ2DJ"`로 고정하는 것도 이 작업 범위 밖입니다. FAT32 조회가 대소문자를 무시하므로 1st SE의 `ez2dj`에도 맞아 현재 결함이 아니며, 프로파일 유도로 바꾸는 것은 별도 작업입니다.

### 검증

- `re2dj_windows_vfs_runtime_probe`에 게스트 루트 접두사 시험을 더합니다. 기본값 `D:\ez2dj`와 설정값 `C:\ez2dj` 양쪽에서 루트 인식이 성립해야 합니다.
- `re2dj ez2dj1stse` 실제 실행에서 `set:request=c:\ez2dj`가 `success=1`이 되고, `find-first`의 `chd_dir`이 `EZ2DJ/System/Title`이 되어야 합니다.
- 3rd·4th 실행의 VFS 경로 해석이 바뀌지 않아야 합니다.

## English

### Purpose

After the decrypted 1st SE guest began executing in the [Hardlock candidate judgement](20260908-224-ez2dj1stse-hardlock-candidate-judgement.md), two working-directory calls were observed to fail. This task fixes the cause of each.

### Defect 1 — the guest root prefix is hardcoded to `D:\ez2dj` — confirmed

`StripGuestRoot` recognizes only the mapped HDD root and the literal `"D:\\ez2dj"`. The 1st SE CHD build's guest boot path is `C:\ez2dj` and it calls `SetCurrentDirectory("c:\ez2dj")`. That name is not recognized as a root, so it parses as a drive-absolute path, has its kind overwritten to relative, combines with an empty base, and reduces to the single component `ez2dj`. Since the VFS root already *is* that `ez2dj` directory, `GuestDirectoryExists("ez2dj")` is false and the call fails — exactly the observed `resolved=ez2dj:success=0`.

The drive letter is a property of the dump, not of the product, and `TargetProfile` already carries `guest_drive_letter` and `guest_directory`, so the runtime should receive that value.

### Defect 2 — CHD enumeration loses the guest current directory — confirmed

The CHD branch of `Re2djVfsFindFirstFileA` passes the whole name, wildcard included, to `ResolveGuestRelativePath`. `ParseGuestPath` rejects `*` and `?` as filename characters in `IsForbiddenComponentCharacter`, so resolution fails and the else branch splits the raw name instead. The directory part comes out empty, `chd_dir` becomes the CHD root `EZ2DJ`, and the current directory is gone.

The same function's native branch does not have this defect: `MapVfsSearchPath` **splits the pattern off first**, maps only the directory, and substitutes `"."` when there is none. That is why the enumeration test in `re2dj_windows_vfs_runtime_probe`, which runs without a CHD, has been passing while the defect persisted on the CHD path.

The parser is right to reject wildcards; the fault is on the calling side, for resolving before splitting.

### Policy changes

Add an exported `g_re2dj_vfs_guest_root` holding the one name by which the guest calls its own root. The launcher fills it from the profile's `guest_drive_letter` and `guest_directory`, falling back to the historical `D:\ez2dj` when the profile carries none, and writes the same value at both injection points — the main process and the bootstrap child. `StripGuestRoot` then consults that single value instead of a literal. The CHD branch of `Re2djVfsFindFirstFileA` splits the pattern first and resolves only the directory, using `"."` when there is none, matching `MapVfsSearchPath`.

Targets whose profiles carry no guest path — 1st, 2nd, 3rd, 4th, 5th, and 6th — receive `D:\ez2dj` and are unchanged. Only 1st SE moves to `C:\ez2dj`.

### What is not changed

Resolving `SetCurrentDirectory("Songs")` to `System/Title/Songs` is **correct Win32 behavior**: a relative path resolves against the current directory. That call is **inferred** to have failed because the preceding enumeration swept the CHD root, so the guest saw the root's `Songs` entry; fixing defect 2 changes the listing the guest sees. Relative-path semantics stay as they are, and the reached boundary is re-observed after the fix.

The `"EZ2DJ"` literal that `ChdRelativePath` and `GuestDirectoryExists` use for the game directory inside the CHD is also out of scope. FAT32 lookup is case-insensitive so it matches 1st SE's `ez2dj` as well, making it not a current defect; deriving it from the profile is separate work.

### Verification

Extend `re2dj_windows_vfs_runtime_probe` with a guest-root-prefix test that passes for both the default `D:\ez2dj` and a configured `C:\ez2dj`. In a real `re2dj ez2dj1stse` run, `set:request=c:\ez2dj` must report `success=1` and the `find-first` line must show `chd_dir=EZ2DJ/System/Title`. VFS path resolution for 3rd and 4th must be unchanged.
