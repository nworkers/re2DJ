# 원본 HDD 디렉터리 준비 절차 / Preparing the Original HDD Directory

사용자가 직접 수행하는 절차다. 원본 EZ2DJ HDD 내용을 디렉터리로 준비하고, re2DJ가 그것을 올바르게 읽는지 확인한다.

*A procedure the user runs themselves: prepare the original EZ2DJ HDD contents as a directory and confirm that re2DJ reads it correctly.*

근거 문서 / Basis: [설계](../design/20260822-003-hdd-directory-input.md), [작업 로그](../work-logs/20260822-003-hdd-directory-input.md), [분석](../analysis/ez2dj-hdd-layout.md)

---

## 1. 디렉터리로 준비 / Extract to a directory

re2DJ는 디스크 이미지를 마운트하지 않는다. 이미지를 먼저 풀어 놓고 그 디렉터리를 가리킨다.

*re2DJ does not mount disk images. Extract the image first and point at the resulting directory.*

* 이미 파일 형태로 가지고 있다면 그대로 쓴다.
* `.img`, `.vhd`, `.vmdk` 같은 이미지라면 마운트하거나 추출 도구로 풀어 디렉터리로 만든다.
* 경로는 저장소 밖이어도 되고, 공백이나 한글이 들어가도 된다.

*If the files are already on disk, use them as they are. If it is an image, mount or extract it into a directory. The path may live anywhere, including outside the repository, and may contain spaces or Korean characters.*

> [!IMPORTANT]
> 이 디렉터리는 원본 그대로 유지한다. re2DJ는 이 경로를 읽기 전용으로 취급하며, 게스트가 파일을 쓰기 시작하면 별도 overlay 디렉터리에 기록한다.
>
> *Keep this directory pristine. re2DJ treats the path as read-only, and once the guest starts writing files they go to a separate overlay directory.*

---

## 2. 내용 확인 / Inspect the contents

```bash
re2dj_hdd_probe /path/to/ez2dj_hdd
```

출력에서 확인할 것:

| 항목 | 기대값 | 어긋날 때 |
| --- | --- | --- |
| `directories`, `files` | 0보다 큼 | 경로를 잘못 지정했거나 빈 디렉터리다 |
| `truncated` | `no` | `--depth`를 올려 다시 실행한다 |
| `executables` | 1 이상 | 실행 파일이 없다. 하위 디렉터리를 지정했을 수 있다 |
| `guest fmt : yes` 인 항목 | 1개 이상 | 32비트 x86 실행 파일이 없다. 아래 3절을 본다 |
| `target profiles` | 1개 이상 | 같음 |

*Check that the counts are non-zero, that `truncated` reads `no`, that at least one executable is listed, and that at least one entry shows `guest fmt : yes` so a target profile exists.*

---

## 3. 게스트 형식 실행 파일이 없을 때 / When no guest-format executable is found

`guest fmt : no`만 나온다면 `pe` 줄을 본다.

| `pe` 줄 | 뜻 | 대응 |
| --- | --- | --- |
| `PE32+ amd64 ...` | 64비트 실행 파일 | 게임 실행 파일이 아니다. 다른 디렉터리를 확인한다 |
| `PE32 i386 ... dll` | 32비트 DLL | 실행 파일이 아니다 |
| `unreadable (missing MZ signature)` | PE가 아니다 | 압축·암호화되었거나 다른 형식이다 |
| `unreadable (missing PE signature)` | DOS 실행 파일 | 이 프로젝트의 대상이 아니다 |

덤프 최상위가 아니라 파티션 하위 디렉터리를 지정했을 가능성도 확인한다.

*If only `guest fmt : no` appears, read the `pe` line: an amd64 image or a DLL is not the game, and an unreadable header means the file is not a PE at all. Also check whether a partition subdirectory was given instead of the dump root.*

---

## 4. 타깃 선택 확인 / Confirm target selection

```bash
re2dj --hdd /path/to/ez2dj_hdd
```

후보가 여럿이면 `--list-targets`로 나열하고 `--target <id>`로 고른다. 백업 폴더에 같은 이름의 실행 파일이 있으면 두 번째 이후 항목에 `_2` 같은 접미사가 붙는다.

*With several candidates, list them with `--list-targets` and select one with `--target <id>`. A duplicate executable name in a backup folder gets a `_2`-style suffix on the later entry.*

> [!WARNING]
> **기본 선택을 믿지 마십시오.** 현재 후보 순위는 파일 크기 내림차순이라, 서비스·테스트 도구가 게임보다 크면 그것이 먼저 선택됩니다. EZ2DJ 1st Trax Special Edition 덤프가 정확히 그 경우로, 기본값이 `Test.exe`가 됩니다. 게임은 `--target ez2dj1`(보호되지 않은 빌드) 또는 `--target ez2dj`입니다. 알려진 결함이며 [TODO](../TODO.md)에 기록되어 있습니다.
>
> ***Do not trust the default selection.** Candidates currently break ties by descending file size, so a service or test tool larger than the game is chosen first. The EZ2DJ 1st Trax Special Edition dump is exactly that case and defaults to `Test.exe`; the game is `--target ez2dj1` (the unprotected build) or `--target ez2dj`. This is a known defect recorded in [TODO](../TODO.md).*

---

## 5. 경로 해석 확인 / Confirm path resolution

원본은 Windows에서 동작했으므로 게임 코드가 실제 파일명과 다른 대소문자로 파일을 연다. 이 확인은 특히 **Linux와 Web 호스트에서 중요하다.**

*The original ran on Windows, so game code opens files with a case that need not match the real name. This check matters most on **Linux and Web hosts**.*

```bash
re2dj --hdd /path/to/ez2dj_hdd --resolve "C:\EZ2DJ\data\song01.ez"
```

`host path`가 실제 파일을 가리키면 정상이다. 출력은 요청한 철자가 아니라 **디스크에 있는 철자**로 나오므로, 같은 덤프에 대해 Windows와 Linux가 같은 값을 낸다. `<not found>`가 나오면 그 경로가 덤프에 없다는 뜻이다.

*The `host path` line pointing at a real file means resolution works. It carries the **on-disk** spelling rather than the requested one, so Windows and Linux report the same value for one dump. `<not found>` means the path is not in the dump.*

---

## 6. 종료 코드 / Exit codes

자동화에서 참고한다.

| 코드 | 뜻 |
| --- | --- |
| 0 | 성공 |
| 1 | 잘못된 사용, 또는 루트를 벗어나는 경로 |
| 2 | HDD 디렉터리 오류, 또는 해석 실패 |
| 3 | 아직 구현되지 않은 기능 (`--run`) |
