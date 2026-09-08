# 작업 로그: 프로파일 API VFS 경계

## 한국어

### 관련 문서

- 설계: [프로파일 API VFS 경계 설계](../design/20260908-233-profile-api-vfs.md)
- 작업 지시: [프로파일 API VFS 경계](../work-orders/20260908-233-profile-api-vfs.md)
- 선행 작업: [예외 코드별 크래시 포렌식](20260908-232-crash-forensics-per-code.md), [LoadImageA 동적 해석 연결](20260908-231-loadimagea-dynamic-resolver.md)

### 해결한 문제

사용자가 보고한 세 가지 중 두 번째와 세 번째, 즉 StreetMix 선택 시 검은 화면과 비정상 종료입니다. 둘은 같은 사건이었습니다. 프로세스가 죽어 present가 멈추니 마지막 화면이 검게 남은 것입니다.

### 진단 경로

[작업 232](20260908-232-crash-forensics-per-code.md)에서 예외 코드별 포렌식을 켜 충돌 지점을 확보했습니다. RVA `0x0002d1f9`의 `idiv dword ptr [ecx+0x17490]`이고 제수는 `[this]`를 3으로 클램프한 값입니다.

이후 이 작업에서 크래시 시점 진단을 두 가지 더 붙여 좁혔습니다.

- `ecx_words`가 `[this]`부터 32바이트를 전부 0으로 보고했습니다. 객체가 초기화되지 않았다는 뜻입니다.
- 0으로 나눈 명령의 변위(`0x17490`)를 복호화된 `.text`에서 스캔해 참조 4곳을 찾았습니다. `0x0002c4c5`와 충돌 함수 자신의 세 곳뿐이며, 객체 주소를 상수로 참조하는 코드는 없습니다.

이 시점에서 자산 쪽을 다시 보고 `ez2dj/Songs/music.ini`가 67개 곡 section을 담은 색인이라는 것을 확인했습니다. 그런데 크래시가 나던 실행의 VFS 추적에 `Songs\` 요청이 **0건**이었습니다.

dynamic resolver 기록을 확인하니 `GetPrivateProfileIntA`, `GetPrivateProfileStringA`, `GetPrivateProfileSectionNamesA`가 모두 `route=win32`였습니다. 실제 Win32 프로파일 API는 VFS도 CHD도 모르므로 호스트에서 이름을 찾고, 찾지 못하면 오류 없이 호출자의 기본값을 돌려줍니다. 프로파일 API는 파일을 내부적으로 열기 때문에 후킹된 `CreateFileA`를 거치지 않아 추적에도 흔적이 없었습니다.

**확인됨.** 곡 목록 열거가 비어 곡 개수가 0이 되고, 그 값이 제수가 되어 0으로 나눕니다.

### 코드 변경

| 항목 | 내용 |
| --- | --- |
| `ResolveProfilePath` | 프로파일 파일 이름을 VFS로 해석하고 필요하면 CHD에서 staging으로 materialize |
| `Re2djVfsGetPrivateProfileIntA` 외 3개 | `StringA`, `SectionNamesA`, `SectionA` wrapper 추가. 해석된 경로로 실제 API 호출 |
| `ReportProfileRead` | api·section·key·요청 경로·해석 경로·결과를 file-event 예산 안에서 기록 |
| dynamic resolver | 네 이름에 HLE를 돌려줌 |

INI 파싱은 다시 구현하지 않고 경로만 바꿉니다. 파일 내용이 원본 그대로이므로 주석·인용·중복 키 처리에서 원본과 어긋날 위험을 만들지 않습니다.

정적 IAT 패치로는 해결되지 않습니다. `.protect` packer가 unpack 시 원본 import를 스스로 해석하며 슬롯을 덮어쓰므로, [작업 231](20260908-231-loadimagea-dynamic-resolver.md)의 `LoadImageA`와 같은 이유로 dynamic resolver가 유일한 연결 지점입니다.

### 검증 — 결과

`re2dj ez2dj1stse`에 코인 3회와 start를 넣고 LEVEL SELECT 타이머를 만료시켜 StreetMix로 진입시켰습니다.

**크래시가 사라지고 게임플레이 화면에 도달합니다.** 노트 레인과 낙하 노트, 그루브 미터, 점수 `003440`, MAX COMBO, `1P PEDAL`, BASS·TREBLE·BOOST·AUDIENCE 이펙터, `CREDITS 1(3/2)`가 모두 표시됩니다.

추적 기록은 다음과 같습니다.

| 항목 | 값 |
| --- | --- |
| `GetPrivateProfileStringA` | 269 |
| `GetPrivateProfileIntA` | 130 |
| `GetPrivateProfileSectionNamesA` | 1 |
| `music.ini` 관련 읽기 | 375 |
| `Songs\` 자산 열기 | 182 |
| `crash-exception` | 1 (진입부 anti-debug `0xC0000005`만) |

첫 호출이 `GetPrivateProfileSectionNamesA(Songs\music.ini)`이고, 이어 `reggae-ez` 같은 section에서 `level`, `clubmixlevel`, `bpm`을 읽는 것이 확인됩니다.

### 검증 — 시험과 회귀

- Windows x86 Release 전체 build 성공
- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → 4개 항목 ok
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0
- 3rd·4th를 40초 제한으로 실행. 두 경우 모두 정상 실행 중이고 `profile-read` 기록이 변경 전후 0건이며 VFS 추적 앞 120줄이 동일합니다. 두 제품은 이 경로를 쓰지 않습니다.

### 재현 도구

키 입력이 가능한 재현 스크립트를 만들었습니다. `config/ez2dj-io.example.ini`를 `--io-config`로 넘기고 `keybd_event`로 coin(`F5`)과 p1 start(`1`)를 보내며, `SetProcessDPIAware` 후 창의 client rect를 캡처합니다. DPI 배율을 반영하지 않으면 좌표가 어긋나 창 밖이 찍힙니다. 이 스크립트는 저장소에 넣지 않고 로컬 임시 경로에 두었습니다.

### 남은 과제

- 보고된 첫 번째 문제인 Warning→로고 깜빡임. ddraw 추적에 그리기 없이 Flip이 연속되는 구간이 있지만 `Present`가 매번 다시 그리므로 그 자체는 원인이 아닙니다.
- 게임플레이 화면 배경이 단색 위 실루엣으로 보입니다. 의도된 BGA인지 color key 처리 문제인지 확인하지 않았습니다.
- 프로파일 쓰기(`WritePrivateProfileStringA`)는 여전히 실제 API로 갑니다. 게스트가 통계를 기록하므로 overlay 정책과 함께 정해야 합니다.
- 정적 IAT 패치가 packed build에서 무효가 되는 다른 경계 점검. `LoadImageA`와 프로파일 API가 같은 이유로 걸렸으므로 더 있을 수 있습니다.

## English

### Related documents

- Design: [Profile API VFS Boundary Design](../design/20260908-233-profile-api-vfs.md)
- Work order: [Profile API VFS Boundary](../work-orders/20260908-233-profile-api-vfs.md)
- Preceding tasks: [per-exception-code crash forensics](20260908-232-crash-forensics-per-code.md), [LoadImageA dynamic resolution](20260908-231-loadimagea-dynamic-resolver.md)

### Problem solved

The user's second and third reported problems — a black screen and an abnormal termination when StreetMix is selected. They are one event: the process died, presentation stopped, and the last frame stayed on screen.

### Diagnostic path

[Task 232](20260908-232-crash-forensics-per-code.md) enabled per-exception-code forensics and located the fault at RVA `0x0002d1f9`, `idiv dword ptr [ecx+0x17490]`, whose divisor is `[this]` clamped to at most 3.

Two further crash-time diagnostics narrowed it. The `ecx_words` record showed the first 32 bytes from `[this]` all zero, meaning the object was never initialised. Scanning the decrypted `.text` for the faulting instruction's displacement found only four references — one at `0x0002c4c5` and three in the faulting function itself — and no code referencing the object's address as a constant.

Returning to the assets showed that `ez2dj/Songs/music.ini` is the song index with 67 sections, yet the crashing run's VFS trace contained **zero** `Songs\` requests. The dynamic-resolver record then showed `GetPrivateProfileIntA`, `GetPrivateProfileStringA`, and `GetPrivateProfileSectionNamesA` all at `route=win32`. The real Win32 profile APIs know nothing of the VFS or the CHD, look the name up on the host, and return the caller's default without an error; because they open the file internally, they leave no trace through the hooked `CreateFileA`.

**Confirmed.** The song enumeration came back empty, the song count was zero, and that value became the divisor.

### Code change

A shared `ResolveProfilePath` resolves a profile file name through the VFS, materialising it from the CHD into the staging tree when needed. Wrappers for `GetPrivateProfileIntA`, `GetPrivateProfileStringA`, `GetPrivateProfileSectionNamesA`, and `GetPrivateProfileSectionA` call the real API with that resolved path, `ReportProfileRead` records each read under the existing file-event budget, and the dynamic resolver answers all four names with these entry points.

Only the path is redirected; INI parsing is not reimplemented, so there is no risk of diverging from the original on comments, quoting, or duplicate keys.

A static IAT patch would not work: the `.protect` packer resolves the original imports itself at unpack time and overwrites the slots, so as with `LoadImageA` in [task 231](20260908-231-loadimagea-dynamic-resolver.md) the dynamic resolver is the only connection point.

### Verification — result

Feeding `re2dj ez2dj1stse` three coins and a start, then letting the LEVEL SELECT timer expire into StreetMix, **the crash is gone and the run reaches the gameplay screen**: note lanes with falling notes, the groove meter, a score of `003440`, MAX COMBO, `1P PEDAL`, the BASS, TREBLE, BOOST, and AUDIENCE effectors, and `CREDITS 1(3/2)`.

The trace records 269 `GetPrivateProfileStringA`, 130 `GetPrivateProfileIntA`, and one `GetPrivateProfileSectionNamesA` call, 375 lines referencing `music.ini`, 182 `Songs\` asset opens, and a single `crash-exception` — the entry-time anti-debug `0xC0000005`. The first call is `GetPrivateProfileSectionNamesA(Songs\music.ini)`, followed by reads of `level`, `clubmixlevel`, and `bpm` from sections such as `reggae-ez`.

### Verification — tests and regression

The full Windows x86 Release build succeeded, `re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, the product-loader probe reported all four items ok, and `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0. 3rd and 4th were each run under a 40-second bound, both still running, with zero `profile-read` records before and after and identical first 120 trace lines; neither uses this path.

### Reproduction tooling

A scripted-input harness was built for this: it passes `config/ez2dj-io.example.ini` through `--io-config`, sends coin (`F5`) and p1 start (`1`) with `keybd_event`, and captures the window's client rect after `SetProcessDPIAware` — without DPI awareness the coordinates are logical while the copy is physical and the grab lands outside the window. The script lives in a local temporary path, not the repository.

### Remaining work

- The first reported problem, the Warning-to-logo flicker. The ddraw trace shows a stretch of consecutive Flips with no drawing between them, but `Present` redraws every time, so that alone is not the cause.
- The gameplay background renders as silhouettes over a flat colour; whether that is the intended BGA or a colour-key issue is unexamined.
- Profile writes such as `WritePrivateProfileStringA` still reach the real API. The guest records statistics, so this needs an overlay policy decided with it.
- Audit other boundaries whose static IAT patch is overwritten on a packed build; `LoadImageA` and the profile APIs both failed this way, so more may remain.
