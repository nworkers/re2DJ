# 원본 실행 파일 분석 (한국어)

이 문서는 원본 EZ2DJ 실행 파일에서 확인한 구조와 설계를 **누적**한다. 영어판은 [EXE_DESIGN.en.md](EXE_DESIGN.en.md)이며 두 문서는 같은 사실을 담는다.

## 표기 규칙

| 표기 | 의미 |
| --- | --- |
| **확인됨** | 실제 바이너리나 실행 결과로 검증했다. 검증 방법을 함께 적는다. |
| **추정** | 근거는 있으나 아직 검증하지 않았다. 근거를 함께 적는다. |
| **미확정** | 아직 모른다. 확인할 방법을 함께 적는다. |

근거 없는 서술을 넣지 않는다. 하나라도 확인되면 그 항목의 표기를 바꾸고 검증 방법을 남긴다.

---

## 1. 현재 상태

EZ2DJ 1st Trax Special Edition과 3rd Trax 덤프 두 개를 확인했다. 정적 분석으로 확인할 수 있는 항목은 대부분 채워졌고, 실행해야 알 수 있는 항목이 남아 있다.

상세 근거는 [HDD 레이아웃 분석](analysis/ez2dj-hdd-layout.md)과 [import 표면 분석](analysis/ez2dj-import-surface.md)에 있다. 여기에는 결론만 둔다.

---

## 2. 확인된 항목

### 2.1 실행 파일 식별 — 확인됨

| 항목 | 값 |
| --- | --- |
| 1st SE 게임 실행 파일 | **`ez2dj.exe`** — `System.ini`의 `shell=` 항목이 가리키는 것 (보호됨) |
| 1st SE bring-up 빌드 | `ez2dj1.exe` (보호되지 않음). 캐비닛이 실행한 것은 아니다 |
| 3rd 게임 실행 파일 | `EZ2DJ.EXE` (보호됨) |
| PE magic | PE32 (`0x10B`) — 전부 |
| machine | i386 (`0x014C`) — 전부 |
| image base | `0x00400000` — 전부 |
| subsystem | Windows GUI (2) — 전부 |
| `.reloc` | 있음 |
| 빌드 시각 | `ez2dj1.exe` 1999-12-24, `ez2dj.exe` 2000-01-01, `EZ2DJ.EXE` 2001-09-24 |
| 보호 여부 | `ez2dj1.exe`만 보호되지 않음. 나머지는 진입점이 `.gtide` / `.protect` 섹션에 있다 |

**`ez2dj1.exe`가 Stage 2·3의 첫 실행 대상이다.** 보호 계층을 실행하지 않고 진짜 게임 코드에 도달할 수 있는 유일한 빌드다.

### 2.2 import 목록 — 확인됨

`ez2dj1.exe` 기준 **7개 DLL, 144개 함수.** 전체 목록과 우선순위는 [import 표면 분석](analysis/ez2dj-import-surface.md)에 있다.

| 항목 | 값 |
| --- | --- |
| 그래픽 | **DirectDraw 1~6** (`DirectDrawCreate`) + GDI DIB 섹션. **Direct3D 없음** |
| 오디오 | **DirectSound** (ordinal `#1`) + `winmm` 믹서 볼륨 |
| 입력 | **`GetAsyncKeyState` 하나가 전부. DirectInput 없음** |
| 설정 | `GetPrivateProfile*` / `WritePrivateProfileStringA` — INI |
| 레지스트리 | `RegFlushKey` 하나 |
| 문자 인코딩 | 전부 ANSI(`...A`) API |
| 스레드 | `CreateThread`, 이벤트, 임계 구역, TLS — **멀티스레드** |
| ordinal import | **사용함** (`DSOUND.dll #1`) |
| delay import | 사용하지 않음 |

3rd는 `DINPUT.dll`, `AVIFIL32.dll`, `WS2_32.dll`을 추가로 쓴다. 버전별 HLE 프로파일이 필요하다.

### 2.3 자산과 런타임 경로 — 부분 확인

| 항목 | 상태 |
| --- | --- |
| HDD 디렉터리 구조 | **확인됨** — 1st SE와 3rd가 서로 다르다 |
| 자산 구성 | **확인됨** — 1st SE는 `Songs/`(68개)와 화면별 `System/` |
| 설정 파일 | **확인됨** — `ez2dj.ini`, `System.ini` |
| 점수 저장 | **확인됨** — `rank_0.dat` ~ `rank_2.dat` (각 400 B) |
| 게스트 작업 디렉터리 | **확인됨(1st SE)** — `\ez2dj`. `System.ini`의 `shell=d:\ez2dj\ez2dj.exe`. 다만 `SetCurrentDirectoryA`를 부르므로 실행 중에 바뀔 수 있다 |
| 드라이브 문자 | **확인됨(1st SE)** — `D:`. 같은 근거 |
| 3rd의 게스트 경로 | **미확정** — 3rd 덤프에는 `System.ini`가 없다 |
| 자산 파일 형식 | **미확정** — `Songs/` 아래 파일 구조는 아직 열어 보지 않았다 |

### 2.4 하드웨어 경계 — 미확정

| 항목 | 상태 |
| --- | --- |
| 아케이드 I/O 보드 | **부분 확인** — 3rd의 `EZ2DJ.INI`에 `"UseIOCard" = 1`이 있어 I/O 카드 사용은 확실하다. 접근 경로는 미확정이다. 1st SE 덤프에 `Tdsd.vxd111`(Windows 9x VxD)이 있으나 `ez2dj1.exe`는 `DeviceIoControl`을 import하지 않고, 확장자 `111`은 비활성화를 시사한다 |
| 동글·보호 장치 | **미확정** — 실행 중 실패 지점을 추적해야 한다 |
| 타이머 소스 | **확인됨** — `timeGetTime` |

---

## 3. 갱신 규칙

* 새 사실을 확인하면 같은 작업에서 이 문서와 [EXE_DESIGN.en.md](EXE_DESIGN.en.md)를 함께 갱신한다.
* 주제별 상세 근거는 `docs/analysis/` 아래 문서에 두고 여기서는 결론과 링크만 남긴다.
* 바이트 열 전체를 옮겨 적지 않는다. 구조, 오프셋, 관찰된 동작만 기록한다.
