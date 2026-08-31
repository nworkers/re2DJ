# 작업 로그: 내장 타깃 프로파일

## 결과

완료. 기본 타깃 선택 결함이 해소됐다.

| 덤프 | 이전 기본 타깃 | 현재 기본 타깃 |
| --- | --- | --- |
| The 1st Tracks Special Edition | `Test.exe` (서비스 도구) | `ez2dj1stse` → `ez2dj.exe` |
| 3rd Trax | `EZ2DJ.EXE` (우연히 맞음) | `ez2dj3rd` → `EZ2DJ.EXE` |

## 사용자 지적이 바꾼 것

작업 시작 시점에 나는 `ez2dj1.exe`를 첫 대상으로 잡고 있었다. 보호되지 않아 로더를 붙이기 쉽다는 이유였다. 사용자가 "실행 파일 이름은 `ez2dj.exe`여야 맞지 않겠어?"라고 지적했고, 확인해 보니 그 지적이 맞았다.

1st SE 덤프의 `System.ini` `[boot]` 절에 결정적 증거가 있었다.

```ini
shell=d:\ez2dj\ez2dj.exe
```

Windows 9x는 이 항목이 가리키는 프로그램을 Explorer 대신 띄운다. 아케이드 캐비닛이 부팅 후 실행하는 것이 `ez2dj.exe`라는 뜻이다.

"구현하기 쉬운 것"과 "원본이 실제로 실행한 것"을 구분하지 못하고 있었다. 전자를 기본값으로 두면 나중에 `ez2dj1.exe`로 관찰한 동작이 원본 동작으로 인용되기 시작한다. 프로젝트 헌장이 "원본 실행 파일을 권위 있는 구현으로 취급한다"고 말하는 바로 그 지점이다.

그래서 프로파일을 둘로 나눴다. `ez2dj1stse`가 정식이자 기본값이고, `ez2dj1stse_unpacked`는 `bring_up_target` 플래그와 함께 "캐비닛이 실행한 것이 아니다"를 프로파일 자체에 기록한다. CLI 목록에 `bring-up only`로 표시되고 선택 시 note가 출력된다.

## 덤으로 확정된 것

`shell=d:\ez2dj\ez2dj.exe` 한 줄이 세 가지를 동시에 확정했다.

| 항목 | 값 | 이전 상태 |
| --- | --- | --- |
| 정식 실행 파일 | `ez2dj.exe` | 추측 중이었음 |
| 게스트 드라이브 문자 | `D:` | **미확정** |
| 게스트 작업 디렉터리 | `\ez2dj` | **미확정** |

드라이브 문자와 작업 디렉터리는 `docs/EXE_DESIGN.*`에서 "실행 중 경로 요청을 추적해야 한다"고 적어 둔 항목이었다. 실행 backend 없이 확정됐다.

3rd 덤프에는 `System.ini`가 없어 같은 값을 확인할 수 없다. **복사해 넣지 않고 비워 뒀다.** `guest_drive_letter`가 `'\0'`이면 CLI가 `<not known for this dump>`로 출력한다.

3rd의 `EZ2DJ.INI`에서 `"UseIOCard" = 1`도 확인해 분석 문서에 반영했다.

## 설계에서 판단한 것

**지문 방식.** 프로파일이 덤프를 식별하는 키로 파일 크기나 해시를 쓰지 않았다. 리비전마다 달라져 정상 덤프를 거부하기 때문이다. 실행 파일 이름과 그 옆에 반드시 있어야 하는 항목 목록을 쓴다.

형제 항목이 반드시 필요하다는 것은 경로 해석 규칙에서 나온다. 해석이 대소문자를 무시하므로 3rd의 `EZ2DJ.EXE`와 1st SE의 `ez2dj.exe`가 이름만으로는 구별되지 않는다. 1st SE 지문은 `Songs`를, 3rd 지문은 `FONTKR.DAT`를 요구해 서로소가 된다. 테스트로 양방향을 고정했다.

**중첩 루트.** 실행 파일을 스캔 결과에서 찾고 형제 항목을 그 실행 파일의 디렉터리 기준으로 확인하므로, 사용자가 상위 디렉터리를 지정해도 그대로 걸린다. 별도 처리를 넣지 않았다.

**보호 여부는 상수가 아니라 계산.** 프로파일에 "보호됨"을 적으면 내장 프로파일이 없는 덤프에서는 아무것도 알 수 없다. 진입점이 놓인 섹션 이름을 PE 헤더에서 찾아 `.text`가 아니면 표시한다. 확보한 표본 다섯 개 전부에서 맞았다.

다만 **이것은 휴리스틱이다.** 함수 이름을 `IsProtected`가 아니라 `HasEntryPointOutsideTextSection`으로 두고, 출력도 "outside .text - likely a protection stub"으로 단정을 피했다. 헤더 주석에 반례 두 가지를 적어 뒀다.

**감지는 남긴다.** 내장 프로파일이 없는 버전의 덤프도 계속 동작해야 한다. `DetectTargetProfiles()`에 제외 목록을 넘겨 내장이 가져간 실행 파일만 건너뛰게 했다.

## 구현 중 겪은 것

Python 치환 스크립트로 C++ 소스를 고치다 일부 치환이 **조용히 실패**했다. 셸을 거치며 `\\n`이 `\n`으로 뭉개져 검색 문자열이 파일 내용과 어긋났기 때문이다. 실패한 치환에 예외를 걸어 두지 않아 즉시 드러나지 않았고, 빌드 전에 파일을 확인해서야 발견했다. 남은 수정은 Edit 도구로 처리했다.

## 검증

**단위 테스트 182개 검사, 실패 0.** 이전 119개에서 63개 늘었다. 새 테스트는 두 덤프 레이아웃을 임시 디렉터리에 재현해 다음을 고정한다.

* 1st SE 기본 타깃이 `ez2dj1stse` → `ez2dj.exe`이고 프로파일이 4개다
* `ez2dj1stse`의 `guest_drive_letter`가 `'D'`, `guest_directory`가 `\ez2dj`다
* `ez2dj1stse_unpacked`에 `bring_up_target`이 서 있다
* 3rd 기본 타깃이 `ez2dj3rd`이고 `guest_drive_letter`가 `'\0'`이다
* 두 덤프가 서로의 프로파일에 걸리지 않는다
* 내장이 가져간 실행 파일이 감지 프로파일로 중복되지 않는다
* 중첩 루트(`se/ez2dj/`)에서 경로와 작업 디렉터리가 올바르게 채워진다
* `Songs/`를 지운 덤프는 내장 프로파일을 가져가지 않고 감지로 넘어간다
* 내장 프로파일이 없는 덤프에서 감지가 그대로 동작한다
* 진입점이 `.text` / `.data` / 어느 섹션에도 없는 세 경우

**실제 덤프 검증.** `roms/ez2dj1stse`와 `roms/ez2dj3rd` 각각에서 기본 타깃이 의도대로 잡혔다. 1st SE는 `entry section : .gtide (outside .text - likely a protection stub)`, 3rd는 `.protect`로 표시됐고, `ez2dj1stse_unpacked`는 `.text`로 표시됐다.

**중첩 루트 실제 검증.** 두 덤프를 모두 담은 `roms/`를 루트로 지정하니(446 디렉터리, 41,669 파일) 내장 프로파일 셋이 각자의 하위 경로로 정확히 잡혔다.

**빌드.** `/W4 /permissive- /WX`에서 경고 없이 빌드됐다.

## 미검증

Linux와 Web에서는 실행하지 않았다. 툴체인이 이 환경에 없다.

실행은 여전히 하지 않는다. 실행 backend가 없다.

내장 프로파일은 확인한 덤프 두 개에 대해서만 추가했다. 다른 버전은 덤프를 확인한 뒤에 추가한다.

## Work Log: Built-In Target Profiles

## Result

Complete. The default-target defect is resolved: the 1st SE dump now selects `ez2dj1stse` → `ez2dj.exe` instead of the service tool `Test.exe`, and the 3rd dump selects `ez2dj3rd` → `EZ2DJ.EXE`.

## What the user's question changed

At the start of this task I had `ez2dj1.exe` as the first target, because being unprotected makes it easier to attach a loader to. The user asked whether the executable name should not be `ez2dj.exe`, and checking showed they were right.

The `[boot]` section of the 1st SE dump's `System.ini` reads `shell=d:\ez2dj\ez2dj.exe`, and Windows 9x launches whatever that entry names in place of Explorer — so `ez2dj.exe` is what the cabinet runs after boot.

I had been conflating "easiest to implement" with "what the original actually ran". Making the former the default would eventually have behavior observed through `ez2dj1.exe` cited as original behavior, which is precisely what the project charter's "treat the original executable as the authoritative implementation" exists to prevent.

So the profile was split. `ez2dj1stse` is canonical and default, while `ez2dj1stse_unpacked` carries a `bring_up_target` flag recording in the profile itself that the cabinet never ran it. The CLI marks it `bring-up only` and prints its note on selection.

## What that confirmed as a side effect

That one line settled three things at once: the canonical executable, the guest drive letter `D:`, and the guest directory `\ez2dj`. The latter two were recorded in the `EXE_DESIGN` documents as needing traced path requests during a run, and they are now settled without an execution backend.

The 3rd dump has no `System.ini`, so the same values cannot be confirmed there. They were **left empty rather than copied across**, and the CLI prints `<not known for this dump>`. The 3rd `EZ2DJ.INI` was also found to carry `"UseIOCard" = 1`, which is now in the analysis document.

## Design judgement

**Fingerprints, not sizes or hashes**, because those vary per revision and would reject a legitimate dump. The required siblings are not optional: path resolution is case-insensitive, so `EZ2DJ.EXE` and `ez2dj.exe` are indistinguishable by name, and the two fingerprints are made disjoint by requiring `Songs` for 1st SE and `FONTKR.DAT` for 3rd. Tests pin both directions.

**Nesting needs no special handling**, since matching searches the scan and checks siblings relative to the matched executable's own directory.

**Protection status is computed, not declared**, so a dump with no built-in profile still reveals something. It is a heuristic, which is why the function is named `HasEntryPointOutsideTextSection` rather than `IsProtected`, the printed wording says "likely", and the header comment records two counterexamples.

**Detection is kept**, with an exclusion list so a built-in match is never duplicated.

## Something that went wrong

While editing C++ sources through a Python replacement script, several replacements **failed silently**: the shell collapsed `\\n` to `\n`, so the search strings no longer matched the file. No assertion guarded those replacements, so the failure surfaced only when the files were inspected before building. The remaining edits were made with the editing tool instead.

## Verification

**182 unit test checks pass with no failures**, up from 119. The new tests reproduce both dump layouts in a temporary directory and pin the default target, the guest path fields, the bring-up flag, mutual non-matching of the two dumps, non-duplication of claimed executables, nested-root path filling, fall-through to detection when a fingerprint breaks, detection with no built-in match, and the three entry-point section cases.

**Against the real dumps**, both select the intended default; 1st SE reports `.gtide`, 3rd reports `.protect`, and the unpacked profile reports `.text`. **With `roms/` as the root** — 446 directories and 41,669 files holding both dumps — all three built-in profiles matched at their correct nested paths.

The build is clean under `/W4 /permissive- /WX`.

## Not verified

Nothing was run on Linux or the Web, as neither toolchain is present. Nothing is executed at all, since there is no execution backend. Built-in profiles were added only for the two inspected dumps.
