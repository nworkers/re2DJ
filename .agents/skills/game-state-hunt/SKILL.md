---
name: game-state-hunt
description: Find a game-state variable (autoplay, demo play, and similar switches) inside an original EZ2DJ/EZ2Dancer executable and wire it into re2DJ as a build-bound game control. Use when asked to find autoplay/데모/autoplay 변수/게임 상태 변수 for a target, to repeat the ez2dj3rd autoplay hunt on another version, or to add a `game_controls` entry to a target profile. Works from a decrypted image dump (`--image-dump`), static analysis with bundled Capstone scripts, read-only runtime polling, and a user-driven write test.
---

# 게임 상태 변수 탐색 (autoplay 기준)

`ez2dj3rd`에서 autoplay 플래그를 찾아 OSD 토글로 연결한 절차(작업 292~297)를 다른 버전에 반복하기 위한 스킬이다. 근거와 실제 값은 다음 문서에 있다.

* [3rd 데모 플레이와 설정 레지스트리](../../../docs/analysis/ez2dj3rd-demo-play.md) — 탐색 결과와 확인됨/추정/미확정
* [4th 데모 플레이와 autoplay 플래그](../../../docs/analysis/ez2dj4th-demo-play.md), [5th](../../../docs/analysis/ez2dj5th-demo-play.md) — 이 스킬로 찾은 결과
* [작업 295 로그](../../../docs/work-logs/20260917-295-autoplay-variable-hunt.md), [작업 296 로그](../../../docs/work-logs/20260917-296-autoplay-write-test.md), [작업 297 설계](../../../docs/design/20260917-297-imgui-osd-autoplay.md)
* [실행 중 주 이미지 덤프 절차](../../../docs/guides/decrypted-image-dump.md)

## 먼저 지킬 것

* **AGENTS.md 절차를 따른다.** `main`이면 작업 브랜치를 만들고, 작업 지시서와 작업 로그를 남긴다. 결과는 `docs/analysis/`에 확인됨/추정/미확정으로 적는다.
* **주소는 빌드 하나에만 유효하다.** 모든 결과에 덤프 sidecar의 `timestamp`를 함께 적는다. 같은 제품이라도 빌드가 다르면 다시 찾는다.
* **관찰과 개입을 구분한다.** 정적 분석과 읽기 폴링은 관찰이다. 게스트 메모리 쓰기는 개입이므로 사용자 동의를 받고, 사용자가 게임을 조작하는 동안에만 한다. 데모 플래그처럼 부작용이 확인된 값은 쓰지 않는다.
* **원본 바이트를 문서에 옮기지 않는다.** 주소, 구조, 명령 형태, 관찰된 동작만 적는다. 덤프 파일은 `logs/` 아래에 두고 커밋하지 않는다.
* **Bash 헤어독에서 백슬래시가 줄어드는 환경이다.** 파일을 고치는 스크립트는 헤어독 대신 파일로 만들어 실행한다.

## 도구

모두 `scripts/`에 있고 Python 3와 Capstone(`pip install capstone`, BSD-3-Clause)이 필요하다. 저장소 root에서 `python .agents/skills/game-state-hunt/scripts/<name>.py`로 실행한다. 주소 인자는 VA(image base 이상)와 RVA를 모두 받는다.

| 스크립트 | 용도 |
| --- | --- |
| `find_strings.py DUMP PATTERN [--xrefs] [--exact]` | 문자열 검색과 그 문자열을 참조하는 코드 위치 |
| `settings_registry.py DUMP SETTINGS_VA` | 설정 객체에 묶인 INI 키 전체와 접근자 |
| `xrefs.py DUMP VA` | 전역 주소를 읽고 쓰는 모든 명령을 write/read로 분류 |
| `disasm.py DUMP ADDR [--function]` | 함수 또는 구간 역어셈블, 문자열 피연산자 주석 |
| `paired_writes.py DUMP ADDR` | 함수가 켰다가 끄는 전역(PAIRED)을 직접 쓰기와 one-line setter 양쪽에서 수집 |
| `register_calls.py DUMP FUNC_VA [--args N]` | 등록 함수 호출마다 넘긴 인자 표. 장면 엔진의 장면 이름·콜백·핸들 목록 |
| `callers.py DUMP VA [--context N]` | link thunk를 거친 호출까지 포함한 호출처 |
| `guest_memory.py read/poll/write` | 실행 중 게스트 메모리 읽기·폴링, `--yes`가 있어야 쓰기 |

## 절차

### 1. 덤프를 뜬다

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe <target> --image-dump --image-dump-delay 8000
```

`logs\windows_x86_launcher_probe\<target>\<stamp>.resumed.image.bin`과 `.json`을 쓴다. **`entry` 덤프는 보호 빌드에서 아직 복호화 전이라 쓰지 않는다.** sidecar의 `gaps`가 비어 있는지, `timestamp`가 무엇인지 기록한다.

`run_detached = false`인 프로파일(현재 `ez2dj6th`)은 `resumed` 지점이 동작하지 않는다. 그 경우는 이 스킬의 범위 밖이므로 사용자에게 알린다.

### 2. autoplay가 저장 설정인지 먼저 배제한다

타깃 HDD의 INI 키 이름 하나를 `find_strings.py --exact --xrefs`로 찾고, 참조 코드를 `disasm.py`로 읽어 설정 객체 전역을 찾는다. 3rd·4th는 `push 주소; push "키"; mov ecx, [설정 객체]; call 접근자` 레지스트리였다. 그 전역으로 `settings_registry.py`를 돌려 묶인 키 전체를 얻고 INI 키와 비교한다. 3rd·4th는 24개, 5th는 `AutoScratch`·`AutoPedal`이 더해진 26개였고, INI에만 있는 `UseIOCard`는 코드가 참조하지 않았다. `Auto`로 시작하는 저장 키가 있어도 노트 전체를 치는 autoplay인지는 따로 확인한다. 1st·1st SE는 섹션이 있는 Win32 profile INI를 쓰므로 모양이 다를 수 있다.

**함정:** 코드 참조가 없는 키 문자열(4th의 `AutoPlay` 등)은 게임이 INI 파일을 읽으며 만든 런타임 사본이다. 게임이 그 키를 안다는 증거가 아니다. overlay INI에는 사용자가 시험으로 넣은 줄이 있을 수 있다.

**판정:** 저장 키 중 autoplay가 없으면 런타임 변수다. 있으면 INI HLE 경로를 먼저 검토한다.

### 3. 데모 단서 문자열에서 데모 경로로 들어간다

```text
python find_strings.py DUMP "demo|auto|attract" --xrefs
```

작업 299의 확인 실행에서 관찰한 단서다. **출발점일 뿐 분석 결과가 아니다.**

| 빌드 | 단서 |
| --- | --- |
| 3rd·4th·5th | `..\..\Common\DEMOPLAY.bmp` (세 빌드 모두 이 경로로 확인됨) |
| 1st·1st SE | `DemoGame`, `ClubMixDemoGame`, `ShowDemoPlay` (1st SE는 장면 이름으로 확인됨, 3-1 경로) |
| `ez2d2m` | `DemoGame::OnCreateGame` 등 클래스 메서드 이름 (3-2 경로) |

참조 함수를 `disasm.py --function`으로 읽어 **단서를 쓸지 결정하는 조건 전역**을 찾는다. 3rd에서는 `cmp dword ptr [0x00a2946c], 0`이 오버레이 로드를 막았다.

#### 3-1. 단서가 장면 이름이면 — 장면 엔진 경로

1st SE는 `DemoGame` 같은 문자열이 조건 전역이 아니라 **장면 등록 함수의 인자**로 쓰였다. 참조 코드를 읽어 등록 함수를 찾고 장면 표를 뽑는다.

```text
python register_calls.py DUMP <등록 함수 VA> --args 8
```

1st SE의 인자 순서는 콜백 네 개, 크기, 플래그, 이름, 핸들 전역이었고 세 번째·네 번째 콜백이 초기화·종료였다. 스스로 플레이하는 장면(데모, 조작법 시연 등)의 **초기화 콜백**을 `disasm.py --function`으로 읽어 상수를 쓰는 전역을 모으고, 각 후보를 `xrefs.py`로 확인한다. **판정:** 1은 자동 플레이 장면들의 초기화에서만, 0은 그 장면들의 종료에서만 쓰이고, 읽는 곳이 플레이어 장면이면 autoplay다. 여러 곳에서 여러 값으로 쓰이는 전역(1st SE `[0x01c3f3a0]`, 쓰기 31곳)은 모드 값이지 autoplay가 아니다.

이 경로에서는 켜고 끄는 짝이 한 함수가 아니라 두 콜백에 나뉘므로 5단계 `paired_writes.py`가 잡지 못한다. 7단계부터는 같다.

**변수가 없는 경우.** 장면 표에 `DemoPlayer`처럼 **데모 전용 플레이어 장면**이 따로 있으면 autoplay가 변수가 아니라 장면 코드일 수 있다. 1st Tracks는 데모 초기화가 상수를 쓰는 전역이 없고, 곡 재생기 호출에 `push 1`(데모)과 `push 0`(일반)을 상수로 넘겼다. 재생기가 그 값을 저장하는 전역은 곡 진행 모듈만 읽었고, 곡 도중 1로 써도 변화가 없었다. 이렇게 **일반 플레이어 장면이 후보를 읽지 않으면** 쓰기 시험 한 번으로 확인하고, `game_controls`에 선언하지 않고 분석 문서에 "변수 없음"으로 기록한다. 장면 교체나 코드 패치는 이 스킬의 범위 밖이다.

#### 3-2. 단서가 클래스 메서드 이름이면 — 클래스 쌍 경로

`ez2d2m`은 등록 함수 없이 클래스마다 `OnCreateGame`·`OnDestroyGame`과 `<이름>Director::Create`·`Delete`를 두고, 각 함수가 자기 이름 문자열을 로그 함수에 넘긴다. 데모 클래스와 짝이 되는 일반 클래스(`DemoGame`↔`NormalGame`)의 **초기화 함수 두 개를 나란히 비교**한다. 레지스터 배정이 달라 일반 diff는 소음이 크므로, 전역 주소 참조와 상수 `push`만 추려 비교하면 차이가 드러난다.

`ez2d2m`에서는 일반 쪽에만 있는 `cmp dword ptr [전역], 1` 분기가 나왔고, 그 분기가 데모와 같은 채널 설정을 할지 정했다. **판정:** 데모가 무조건 쓰는 설정을 일반 게임이 그 전역이 참일 때만 쓰면 그 전역이 autoplay다.

**주의:** 이 형태에서는 데모가 플래그를 쓰지 않고 상수를 쓰므로 7단계 읽기 폴링으로 확인할 수 없다. 8단계 쓰기 시험이나 9단계 OSD 확인으로 판정한다. 값을 곡 시작 때 읽으므로 곡 선택 화면에서 쓰고 곡을 시작해야 한다.

### 4. 조건 전역의 쓰기 지점에서 데모 시작 루틴을 찾는다

```text
python xrefs.py DUMP <조건 전역 VA>
```

1을 쓰는 `write` 행의 함수가 데모 시작 루틴 후보다. 3rd는 쓰기 2곳(1과 0)이 한 함수 `0x0048aa31`에 있었다.

### 5. 짝으로 켜고 끄는 전역을 모은다 — 핵심 단계

```text
python paired_writes.py DUMP <데모 시작 루틴 VA>
```

`PAIRED`가 후보다. 3rd에서는 데모 플래그 `[0x00a2946c]`와 함께 setter를 거친 `[0x00a29508]`이 나왔고, 후자가 autoplay였다. `RESET`(0으로만 초기화)은 보통 후보가 아니다.

**함정:** 데모 플래그 자체를 autoplay로 착각하지 않는다. 데모 플래그를 읽는 곳은 오버레이·입력 해제·음소거·입력 시 종료 같은 **장면 동작**이고, 노트를 치는 곳이 아니었다.

### 6. 후보가 노트 판정을 좌우하는지 읽는다

후보를 직접 읽는 곳은 `xrefs.py`로, getter를 거쳐 읽는 곳은 getter에 `callers.py --context 4`로 찾는다. 3rd는 getter `0x004353e0` 호출처 7곳 중 두 곳이 노트 도착·종료 때 **게임이 직접** 키 누름·판정·뗌 연출을 호출했다. 같은 값을 입력 매니저가 슬롯 `0x1b`로 뒤집는 곳도 이 단계에서 나왔다.

**함정:** getter를 노트마다 부른다고 판정이 그 값을 매 프레임 따르는 것은 아니었다. 3rd는 곡 시작 때 값을 고정했다. 이 사실은 7·8단계에서만 드러났으므로 정적 해석으로 단정하지 않는다.

### 7. 읽기 전용으로 런타임 확인

게임을 어트랙트 상태로 두고 데모 플래그와 후보를 함께 폴링한다.

```text
python guest_memory.py poll --process EZ2DJ.EXE --seconds 150 demo=<VA> candidate=<VA>
```

**판정:** 데모가 도는 구간에만 둘이 함께 1이 되고 끝나면 0으로 돌아와야 한다. 3rd는 약 22~25초 데모마다 그랬다. 실행 파일 이름이 다르면 `--process`를 맞춘다.

### 8. 쓰기 시험 — 사용자 동의 후

게임 입력이 필요하므로 io-config를 붙여 실행한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe <target> --io-config .\config\ez2dj-io.example.ini
```

1. 사용자가 코인을 넣고 **곡 선택 화면**까지 간다. 어트랙트 중에는 쓰지 않는다. 데모가 끝나면서 값을 0으로 되돌린다.
2. `python guest_memory.py write --process EZ2DJ.EXE <VA> 1 --yes`
3. 사용자가 곡을 시작하고 노트가 자동으로 맞는지, 데모 오버레이·음소거·입력 시 종료가 없는지 본다.
4. 곡 도중 `0`을 써서 반영 시점도 확인한다. 3rd는 **다음 곡부터** 반영됐다.

**판정:** 입력 없이 노트가 맞고 데모 부작용이 없으면 확인됨으로 기록한다.

### 9. 제품에 연결한다

`src/target/target_profile.cpp`의 해당 프로파일에 선언한다. RVA는 `VA - image_base`, timestamp는 sidecar 값이다.

```cpp
entry.profile.game_controls.autoplay_flag_rva = 0x00629508;   // 3rd 예시
entry.profile.game_controls.build_timestamp = 0x3bca98a3;
```

런처가 실행 파일 timestamp가 같을 때만 주소를 넘기므로 OSD에 토글이 자동으로 나타난다. `tests/unit/target_profile_test.cpp`의 "3rd 외에는 선언이 없다" 목록에서 해당 id를 빼고 값 검사를 추가한다. 빌드·단위 테스트 후 OSD(백틱)에서 토글이 보이는지 사용자와 확인한다. 실행 로그 `osd_controls` 줄의 `autoplay_armed`로도 확인할 수 있다.

### 10. 기록한다

* `docs/analysis/<target>-demo-play.md` — 3rd 문서의 절 구성을 따르고 확인됨/추정/미확정을 구분한다. `docs/analysis/README.md` 색인.
* `docs/EXE_DESIGN.ko.md`·`.en.md`에 확인된 구조 누적.
* 작업 지시서·작업 로그, `docs/IMPLEMENTED.md`, 필요하면 `RELEASE_NOTES.md`.

## 빌드 성격에 따른 주의

* 3rd는 MSVC 디버그 구성이라 `push ebp; mov ebp, esp` 프롤로그와 `jmp` thunk로 구조가 잘 보였다. 최적화된 빌드는 프롤로그가 없을 수 있다. `disasm.py --function`이 프롤로그를 못 찾으면 구간 모드(`--before`/`--after`)로 읽는다.
* `paired_writes.py`는 상수 쓰기와 one-line setter만 인식한다. 멤버 변수, 계산된 값, 인라인된 setter는 놓친다. 결과가 비어도 "없다"가 아니다.
* `xrefs.py`는 절대 주소 참조만 찾는다. 포인터나 객체 멤버를 거친 접근은 getter·setter를 찾아 `callers.py`로 따라간다.
* 입력 매니저 같은 힙 객체는 실행마다 주소가 달라 `--field-write-watch`(진입 시점 고정 주소)에 맞지 않았다.
* `xrefs.py`의 쓰기·읽기 분류는 명령 복원에 달려 있다. 4th의 `mov [addr], eax`(1바이트 opcode 형태)가 한때 `or byte ptr [ebx + addr], ah`로 복원됐던 결함은 고쳤지만, 레지스터가 섞인 이상한 명령이 보이면 `disasm.py`로 그 주변을 직접 읽어 확인한다.

## 확인된 결과

| 타깃 | timestamp | 데모 플래그 | autoplay 플래그 (RVA) | 작업 |
| --- | --- | --- | --- | --- |
| `ez2dj3rd` | `0x3bca98a3` | `0x00a2946c` | `0x00a29508` (`0x00629508`) | 295~297 |
| `ez2dj4th` | `0x3d369bfd` | `0x00ac290c` | `0x00ac29b0` (`0x006c29b0`) | 300 |
| `ez2dj5th` | `0x3f53377b` | `0x00aee194` | `0x00aee238` (`0x006ee238`) | 301 |
| `ez2dj1stse` (CHD) | `0x3862df27` | 장면 `DemoGame`·`ClubMixDemoGame`·`HowToPlayGame` | `0x01c3f3a4` (`0x0183f3a4`) | 302 |
| `ez2dj1st` | `0x3862fd9d` | 전용 장면 `DemoPlayer`·`ClubMixDemoPlayer` | **없음** — 재생기 인자 저장 `[0x0055bc4c]`는 쓰기 시험에서 효과 없음 | 304 |
| `ez2d2m` | `0x3a5f074c` | 클래스 `DemoGame`(플래그 대신 상수 사용) | `0x007fa424` (`0x003fa424`) | 305 |

3rd·4th·5th는 모두 같은 구조였다. getter 호출처의 노트 데이터 필드 오프셋은 빌드마다 다를 수 있다(5th 첫 계열은 `+0x1d8`).
