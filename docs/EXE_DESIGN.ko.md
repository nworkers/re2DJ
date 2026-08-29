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

상세 근거는 [HDD 레이아웃 분석](analysis/ez2dj-hdd-layout.md), [실행 파일 구조 분석](analysis/ez2dj-exe-structures.md), [import 표면 분석](analysis/ez2dj-import-surface.md)에 있다. 실행 파일별 PE 구조·보호 계층 해부·데이터 인벤토리는 구조 문서가 담당하며, 새 실행 파일이 확인될 때마다 그 문서에 섹션이 추가된다. 여기에는 결론만 둔다.

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
| `.reloc` | 섹션은 있으나 `ez2dj1.exe`의 base relocation data directory는 비어 있음 — 선호 주소 고정 |
| 빌드 시각 | `ez2dj1.exe` 1999-12-24, `ez2dj.exe` 2000-01-01, `EZ2DJ.EXE` 2001-09-24 |
| 보호 여부 | `ez2dj1.exe`만 보호되지 않음. 나머지는 진입점이 `.gtide` / `.protect` 섹션에 있다 |

**`ez2dj1.exe`가 Stage 2·3의 첫 실행 대상이다.** 보호 계층을 실행하지 않고 진짜 게임 코드에 도달할 수 있는 유일한 빌드다.

### 2.2 import 목록 — 확인됨

`ez2dj1.exe` 기준 **7개 DLL, 144개 함수.** 전체 목록과 우선순위는 [import 표면 분석](analysis/ez2dj-import-surface.md)에 있다.

| 항목 | 값 |
| --- | --- |
| 그래픽 | **DirectDraw/Direct3D Immediate Mode 계열**. runtime은 `QueryInterface(IID_IDirect3D3)`로 Direct3D를 얻고 XYZ/NORMAL/TEX1 정점 121개를 `CreateVertexBuffer`로 만든 뒤 null-size `Lock`과 stride 32의 11×11 grid fill을 수행한다. 확인된 draw state는 stage-zero texture/diffuse modulate, linear filtering, RGB565 source color key, alpha test와 `ZERO/SRCALPHA`·`ONE/ZERO` blending이다. `%s.bmp` 지연 로딩은 `DDSCAPS_OFFSCREENPLAIN` RGB565 surface를 만들고 GDI 복사 뒤 source-key `BltFast`/`Blt`로 합성한다. 전역 `[0x01eb7cc0]`의 호출 형태는 `IDirect3DDevice3` vtable과 일치한다. |
| 오디오 | **DirectSound** (ordinal `#1`) + `winmm` 믹서 볼륨. 정적 buffer와 360,448바이트 looping ring buffer를 만들며, streaming 경로는 전체 Lock 안에서 45,056바이트 PCM 청크를 순환 갱신한다. `GAMEASSIGNMENTS/DemoVolume` 인덱스 0..3은 `[-10000, -2222, -1111, 0]`의 DirectSound volume table을 선택한다. `DuplicateSoundBuffer`도 사용한다. |
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
| 아케이드 I/O 보드 | **부분 확인** — 3rd의 `EZ2DJ.INI`에 `"UseIOCard" = 1`이 있어 I/O 카드 사용은 확실하다. 1st SE 보호 실행 파일의 byte `IN`/`OUT` port 범위와 active-low bank는 확인됐다. 공개 독립 구현과 교차 확인한 button/turntable/coin/light 의미는 [I/O port map](analysis/ez2dj-io-map.md)에 **추정**으로 분리했다. `OUT 0x106` 의미는 미확정이다 |
| 동글·보호 장치 | **부분 확인** — 보호 stub이 `\\.\LPTDI1`을 열고 4→8바이트, 24→104바이트 IOCTL 두 건을 보낸다. 두 단계 모두 output 첫 DWORD 0이 진행 조건이다. 첫 단계는 최대 3회 반복한다. 두 번째 input DWORD에 `0x01ed4141` 변환을 두 번 적용한 8바이트 mask가 response offset 4~11과 XOR되어 `.data` 복원 상태가 된다. 이 상태의 첫 DWORD는 `0x01ed7296`에 seed되고 같은 변환으로 바이트마다 갱신되며, 하위 바이트가 보호 `.data`에서 빠진다. 최소 target state `0900000000000000`은 정상 initializer를 반복 복원했다. 이는 바이너리 복원값이며 실제 동글 key나 vendor protocol의 확정은 아니다. HASP4 `HaspCode`의 첫 shape 유사성은 있으나 classic HASP 공개 경로·packet과 전체 LPTDI interface가 달라 vendor는 미확정이다 |
| 타이머 소스 | **확인됨** — `timeGetTime` |

---

## 3. 갱신 규칙

* 새 사실을 확인하면 같은 작업에서 이 문서와 [EXE_DESIGN.en.md](EXE_DESIGN.en.md)를 함께 갱신한다.
* 주제별 상세 근거는 `docs/analysis/` 아래 문서에 두고 여기서는 결론과 링크만 남긴다.
* 바이트 열 전체를 옮겨 적지 않는다. 구조, 오프셋, 관찰된 동작만 기록한다.
