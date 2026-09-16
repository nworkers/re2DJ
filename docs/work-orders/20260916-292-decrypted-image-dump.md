# 작업 292 작업 지시 — 복호화된 주 이미지 덤프 / Task 292 work order — Decrypted main-image dump

설계: [20260916-292-decrypted-image-dump.md](../design/20260916-292-decrypted-image-dump.md)

## 한국어

### 목적

보호된 빌드가 실행 중 복호화한 주 이미지를 파일로 남겨, 정적 분석이 불가능했던 대상에 분석 경로를 연다.

### 구현 범위

#### 1. 신규 모듈 `src/platform/windows/process_image_dump.h/.cpp`

덤프 한 건을 쓰는 책임만 가진다. 옵션 파싱도, 시점 판단도 하지 않는다.

| 항목 | 내용 |
| --- | --- |
| 입력 | 프로세스 핸들, image base, `SizeOfImage`, 지점 이름, 출력 경로 두 개, 귀속 정보 |
| 읽기 | `ReadProcessMemory`를 페이지 단위로. 실패한 범위는 0으로 채우고 범위를 기록 |
| 출력 | virtual 레이아웃 그대로의 `.image.bin`, 귀속 정보 `.image.json` |
| 반환 | 성공 여부, 읽은 바이트 수, 실패 범위 목록 |

한 번의 `ReadProcessMemory`로 전체를 읽으려 하지 않는다. 이미지 안에 커밋되지 않은 페이지가 있으면 전체 호출이 실패하므로, 구멍의 위치를 알 수 없게 된다.

#### 2. 귀속 정보

sidecar에 다음을 쓴다. 하나라도 빠지면 덤프를 `docs/analysis/`의 근거로 쓸 수 없다.

* 타깃 id, 게스트 실행 파일 경로
* PE `timestamp`, `size_of_image`, `entry_point_rva` — 프로파일 fingerprint와 같은 값
* 디스크 파일 크기와 다이제스트
* image base, 지점 이름, 지연 시간
* 읽기 실패 범위 목록
* re2dj 버전

다이제스트는 저장소에 암호학적 해시 구현이 없으므로 FNV-1a 64비트를 쓴다. **sidecar에 알고리즘 이름을 함께 적어 암호학적 해시가 아님을 분명히 한다.** 목적은 다른 파일에서 나온 덤프를 알아채는 것이지 위조 방지가 아니다.

#### 3. 런처 probe 옵션

| 옵션 | 동작 |
| --- | --- |
| `--image-dump [path]` | 덤프를 켠다. 경로를 주지 않으면 그 실행의 로그 디렉터리 |
| `--image-dump-delay <milliseconds>` | 지점 B의 시점. 기본 5000 |

옵션이 없으면 어떤 메모리도 읽지 않고 진단 줄도 남기지 않는다.

#### 4. 덤프 지점 두 곳

* **지점 A `entry`** — 진입점 breakpoint 복원 직후, 프로세스가 아직 정지해 있을 때.
* **지점 B `resumed`** — `ResumeThread` 뒤 지정 시간 경과 후. 그 시점에 프로세스가 이미 종료했으면 덤프를 건너뛰고 그 사실을 진단에 남긴다.

지점 B는 detached 실행 경로에 둔다. 디버거를 분리해도 프로세스 핸들은 유효하다.

#### 5. 진단 기록

지점마다 한 줄씩 남긴다. 파일 경로, 지점, 읽은 바이트 수, 실패 범위 개수를 포함한다.

### 검증

1. **대조군 먼저.** 보호되지 않은 6th를 덤프하고 `UseGameOver`, `TotalCoin`, `AdvSound`가 나오는지 확인한다. 디스크 이미지 검색 결과와 일치해야 한다. 정답을 아는 대상으로 덤프 경로 자체를 검증하는 단계이며, **여기서 실패하면 이후 결과는 읽지 않는다.**
2. 3rd를 덤프하고 같은 문자열을 찾는다. 디스크에서는 0건이므로 덤프에서 나오면 복호화 상태를 뜬 것이다.
3. 지점 A와 B의 덤프를 비교해 진입점 시점의 복호화 여부를 판정한다.
4. 옵션 없는 실행이 덤프 파일을 만들지 않음을 확인한다.
5. Windows x86 Debug·Release 빌드와 단위 테스트.

### 완료 조건

* 위 검증 1~4의 결과와 근거.
* 진입점 시점의 복호화 여부 판정.
* `docs/analysis/` — 판정 결과를 확인됨/추정/미확정 구분해 기록. 새 파일을 만들면 `README.md` 색인 갱신.
* `docs/guides/` — 덤프를 뜨는 절차. 반복 수행 가능한 절차이므로 여기에 둔다.
* 작업 로그.

### 범위에서 뺀 것

* 실행 가능한 PE 복원. import 재구성이 필요하며 정적 분석에는 불필요하다.
* 덤프를 이용한 실제 분석. autoplay 변수 추적을 포함해 별도 작업이다.
* 두 지점 외의 추가 시점. 단계적 복호화가 확인되면 그때 넓힌다.

### 실행 중 바뀐 것

* **제품 CLI 노출을 범위에 넣었다.** 처음에는 제외했으나, 보호된 타깃은 모두 CHD 기반이라 런처 probe를 직접 부르려면 CHD 추출 경로까지 손으로 재현해야 한다. 즉 제외하면 **이 작업이 존재하는 이유인 대상에서 기능을 실행할 수 없다.** `--guest-wait-trace`와 같은 방식으로 붙였다.
* **6th 대조군을 대체했다.** 6th는 게임 본체가 자식 프로세스이고 본체를 직접 띄우면 런타임 주입이 실패한다(이번 작업과 무관한 기존 bring-up 미완). 대신 **PE 헤더 대조**를 썼다. 헤더는 보호 계층이 건드리지 않고 파일 오프셋과 RVA가 같으므로, 파일과 덤프가 그 영역에서 일치하는지가 덤프 충실성을 직접 증명한다. 실제로 쓰는 타깃에서 검증한다는 점에서 더 낫다.

## English

Design: [20260916-292-decrypted-image-dump.md](../design/20260916-292-decrypted-image-dump.md)

### Purpose

Save the main image a protected build decrypts at run time, opening an analysis path to targets where static analysis was impossible.

### Scope

#### 1. New module `src/platform/windows/process_image_dump.h/.cpp`

Responsible only for writing one dump: it neither parses options nor decides when to fire.

| Item | Content |
| --- | --- |
| Input | Process handle, image base, `SizeOfImage`, point name, two output paths, attribution |
| Reading | `ReadProcessMemory` page by page; failed ranges are zero-filled and recorded |
| Output | `.image.bin` in virtual layout, `.image.json` with attribution |
| Result | Success, bytes read, the list of failed ranges |

It must not try to read the whole image in one call: an uncommitted page anywhere inside makes the whole call fail, which loses the location of the holes.

#### 2. Attribution

The sidecar carries the target id and guest executable path; the PE `timestamp`, `size_of_image` and `entry_point_rva`, matching the profile fingerprints; the on-disk file size and digest; the image base, point name and delay; the list of failed ranges; and the re2dj version. A dump missing any of these cannot support anything in `docs/analysis/`.

The digest is FNV-1a 64-bit, since the repository has no cryptographic hash. **The sidecar names the algorithm so it is clear this is not a cryptographic hash**: the purpose is noticing a dump that came from a different file, not resisting forgery.

#### 3. Launcher probe options

`--image-dump [path]` enables the dump, defaulting to that run's log directory, and `--image-dump-delay <milliseconds>` sets point B's timing, default 5000. Without the option no memory is read and no diagnostic line is written.

#### 4. The two points

* **Point A, `entry`** — immediately after the entry breakpoint is restored, while the process is still stopped.
* **Point B, `resumed`** — after `ResumeThread` plus the configured delay. If the process has already exited by then, the dump is skipped and that is recorded.

Point B lives on the detached execution path; detaching the debugger leaves the process handle valid.

#### 5. Diagnostics

One line per point, carrying the file path, the point, the bytes read, and the number of failed ranges.

### Verification

1. **The control first.** Dump the unprotected 6th and confirm `UseGameOver`, `TotalCoin` and `AdvSound` appear, matching what its disk image shows. This validates the dump path itself against a target whose answer is known, and **if it fails, no later result is to be read.**
2. Dump 3rd and search the same strings. The disk image has zero, so finding them proves a decrypted state was captured.
3. Compare the point A and point B dumps to decide whether the entry breakpoint is past decryption.
4. Confirm a run without the option produces no dump file.
5. Windows x86 Debug and Release builds plus unit tests.

### Completion criteria

* The results and evidence for verifications 1-4.
* The decision on whether the entry point is past decryption.
* `docs/analysis/` — the decision recorded as confirmed, inferred or unresolved, with the directory `README.md` index updated if a file is added.
* `docs/guides/` — the procedure for taking a dump, which belongs there as a repeatable procedure.
* A work log.

### Out of scope

* Reconstructing a runnable PE, which needs import rebuilding and is unnecessary for static analysis.
* Actually analyzing the dump, including the autoplay variable hunt; that is its own task.
* Points beyond the two. If staged decryption is confirmed, widen then.

### Changed during the work

* **Product CLI exposure was brought into scope.** It was excluded at first, but every protected target is CHD-based, so calling the launcher probe directly means reproducing the CHD extraction path by hand. Excluding it therefore made the feature **impossible to exercise on the very targets this task exists for.** It is wired the same way `--guest-wait-trace` is.
* **The 6th control was substituted.** 6th runs its game body as a child process, and launching that body directly fails at runtime injection — a pre-existing bring-up gap unrelated to this work. A **PE header comparison** was used instead: the headers are untouched by the protection and share file offset with RVA, so agreement between file and dump over that range proves the dump faithful directly. It is the better control anyway, since it tests a target actually in use.
