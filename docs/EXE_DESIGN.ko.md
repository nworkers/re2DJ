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

EZ2DJ The 1st Tracks Special Edition, 2nd Trax, 3rd Trax, 4th Trax, 5th, 6th 덤프와 EZ2Dancer 2nd MOVE(`ez2d2m`)를 확인했다. 입력 형태는 디렉터리 덤프와 MAME CHD 두 가지다. 정적 분석으로 확인할 수 있는 항목은 대부분 채워졌고, 보호 계층 응답과 실행해야 알 수 있는 항목이 남아 있다.

상세 근거는 [HDD 레이아웃 분석](analysis/ez2dj-hdd-layout.md), [실행 파일 구조 분석](analysis/ez2dj-exe-structures.md), [import 표면 분석](analysis/ez2dj-import-surface.md)에 있다. 실행 파일별 PE 구조·보호 계층 해부·데이터 인벤토리는 구조 문서가 담당하며, 새 실행 파일이 확인될 때마다 그 문서에 섹션이 추가된다. 여기에는 결론만 둔다.

---

## 2. 확인된 항목

### 2.1 실행 파일 식별 — 확인됨

| 항목 | 값 |
| --- | --- |
| 1st SE 게임 실행 파일 | **`ez2dj.exe`** — `System.ini`의 `shell=` 항목이 가리키는 것 (보호됨) |
| 1st Tracks 대표 실행 파일 | **`Ez2DJ.exe`** — 사용자가 지정한 대표 파일 (`.protect`, 보호됨) |
| 2nd Tracks 대표 실행 파일 | **`EZ2DJ.exe`** — 사용자가 제공한 대표 파일 (PE entry point는 `.text`, 보호 여부 미확정) |
| 3rd 게임 실행 파일 | `EZ2DJ.EXE` (보호됨, `.protect`) |
| 4th 게임 실행 파일 | `EZ2DJ.exe` (보호됨, `.protect`) |
| EZ2Dancer 2nd MOVE 실행 파일 | `EZ2Dancer.exe` (보호됨, `.protect`) |
| PE magic | PE32 (`0x10B`) — 전부 |
| machine | i386 (`0x014C`) — 전부 |
| image base | `0x00400000` — 전부 |
| subsystem | Windows GUI (2) — 전부 |
| base relocation | 1st SE `ez2dj.exe`는 `.reloc` 섹션이 있어도 data directory가 비어 있어 선호 주소 고정. 3rd·4th는 `.protect` 안에 실제 relocation directory가 있다 |
| 빌드 시각 (PE TimeDateStamp) | `ez2dj.exe` 1999-12-24, 2nd `EZ2DJ.exe` 2004-07-18, 3rd `EZ2DJ.EXE` 2001-09-24(디렉터리)·2001-10-15(CHD), 4th `EZ2DJ.exe` 2002-07-18, `EZ2Dancer.exe` 2001-01-12 |
| 보호 여부 | 1st SE·3rd·4th·`ez2d2m`은 진입점이 `.gtide` / `.protect`에 있어 보호됨. 2nd는 `.text`에 있어 미확정 |

**정식 실행 파일을 직접 적재한다.** 보호 계층을 우회하는 별도 bring-up 빌드를 쓰지 않고, 캐비닛이 실제로 실행하는 빌드의 packer를 그대로 통과시킨다.

### 2.2 import 목록 — 확인됨

정식 `ez2dj.exe` 기준 loader가 bind하는 표면은 **7개 DLL, 161개 함수**이며 그중 17개가 보호 계층의 것이다. 3rd·4th는 원본 `.idata`가 10 DLL / 159·161이다. 전체 수치와 우선순위는 [import 표면 분석](analysis/ez2dj-import-surface.md)에 있다.

| 항목 | 값 |
| --- | --- |
| 그래픽 | **DirectDraw/Direct3D Immediate Mode 계열**. runtime은 `QueryInterface(IID_IDirect3D3)`로 Direct3D를 얻고 XYZ/NORMAL/TEX1 정점 121개를 `CreateVertexBuffer`로 만든 뒤 null-size `Lock`과 stride 32의 11×11 grid fill을 수행한다. 원본 `0x0042069d`는 전역 device `[0x01eb7cc0]`의 vtable `+0x8c`를 통해 `DrawIndexedPrimitiveVB(D3DPT_TRIANGLELIST, vb, indices, 600, 0)`를 호출한다. 확인된 draw state는 stage-zero texture/diffuse modulate, linear filtering, RGB565 source color key, alpha test와 `ZERO/SRCALPHA`·`ONE/ZERO` blending이다. `%s.bmp` 지연 로딩은 `DDSCAPS_OFFSCREENPLAIN` RGB565 surface를 만들고 GDI 복사 뒤 source-key `BltFast`/`Blt`로 합성한다. 전역 `[0x01eb7cc0]`의 호출 형태는 `IDirect3DDevice3` vtable과 일치한다. |
| 오디오 | **DirectSound** (ordinal `#1`) + `winmm` 믹서 볼륨. 정적 buffer와 360,448바이트 looping ring buffer를 만들며, streaming 경로는 전체 Lock 안에서 45,056바이트 PCM 청크를 순환 갱신한다. `GAMEASSIGNMENTS/DemoVolume` 인덱스 0..3은 `[-10000, -2222, -1111, 0]`의 DirectSound volume table을 선택한다. `DuplicateSoundBuffer`도 사용한다. |
| 입력 | **`GetAsyncKeyState` 하나가 전부. DirectInput 없음** |
| 설정 | `GetPrivateProfile*` / `WritePrivateProfileStringA` — INI |
| 레지스트리 | `RegFlushKey` 하나 |
| 문자 인코딩 | 전부 ANSI(`...A`) API |
| 스레드 | `CreateThread`, 이벤트, 임계 구역, TLS — **멀티스레드** |
| ordinal import | **사용함** (`DSOUND.dll #1`) |
| delay import | 사용하지 않음 |

3rd와 4th는 `DINPUT.dll`, `AVIFIL32.dll`, `WS2_32.dll`을 추가로 쓰고 `USER32` 표면도 21에서 32로 커진다. 그래픽 진입점도 `DirectDrawCreate`가 아니라 `DirectDrawCreateEx`다. 버전별 HLE 프로파일이 필요한 이유가 이 차이다.

### 2.3 자산과 런타임 경로 — 부분 확인

| 항목 | 상태 |
| --- | --- |
| HDD 디렉터리 구조 | **확인됨** — 1st SE와 3rd가 서로 다르다 |
| 자산 구성 | **확인됨** — 1st SE는 `Songs/`(68개)와 화면별 `System/` |
| 설정 파일 | **확인됨** — `ez2dj.ini`, `System.ini` |
| 점수 저장 | **확인됨** — `rank_0.dat` ~ `rank_2.dat` (각 400 B) |
| 게스트 작업 디렉터리 | **확인됨(1st SE)** — `\ez2dj`. `System.ini`의 `shell=d:\ez2dj\ez2dj.exe`. 다만 `SetCurrentDirectoryA`를 부르므로 실행 중에 바뀔 수 있다 |
| 드라이브 문자 | **확인됨(1st SE)** — `D:`. 같은 근거 |
| 1st Tracks 게스트 경로 | **미확정** — 입력에 `System.ini`가 없음 |
| 3rd의 게스트 경로 | **미확정** — 3rd 덤프에는 `System.ini`가 없다 |
| 2nd의 게스트 경로 | **미확정** — 2nd 덤프에는 `System.ini`가 없다 |
| 자산 파일 형식 | **미확정** — `Songs/` 아래 파일 구조는 아직 열어 보지 않았다 |

### 2.4 하드웨어 경계 — 미확정

| 항목 | 상태 |
| --- | --- |
| 아케이드 I/O 보드 | **부분 확인** — 3rd의 `EZ2DJ.INI`에 `"UseIOCard" = 1`이 있어 I/O 카드 사용은 확실하다. 1st SE 보호 실행 파일의 byte `IN`/`OUT` port 범위와 active-low bank는 확인됐다. 공개 독립 구현과 교차 확인한 button/turntable/coin/light 의미는 [I/O port map](analysis/ez2dj-io-map.md)에 **추정**으로 분리했다. `OUT 0x106` 의미는 미확정이다 |
| 동글·보호 장치 | **부분 확인** — 보호 stub이 `\\.\LPTDI1`을 열고 4→8바이트, 24→104바이트 IOCTL 두 건을 보낸다. 두 단계 모두 output 첫 DWORD 0이 진행 조건이다. 첫 단계는 최대 3회 반복한다. 두 번째 input DWORD에 `0x01ed4141` 변환을 두 번 적용한 8바이트 mask가 response offset 4~11과 XOR되어 `.data` 복원 상태가 된다. 이 상태의 첫 DWORD는 `0x01ed7296`에 seed되고 같은 변환으로 바이트마다 갱신되며, 하위 바이트가 보호 `.data`에서 빠진다. 최소 target state `0900000000000000`은 정상 initializer를 반복 복원했다. 이는 바이너리 복원값이며 실제 동글 key나 vendor protocol의 확정은 아니다. HASP4 `HaspCode`의 첫 shape 유사성은 있으나 classic HASP 공개 경로·packet과 전체 LPTDI interface가 달라 vendor는 미확정이다. 3rd는 별도 `\\.\FEnteDev` 경계에서 `0x9c402468→450→44c→458` 계약을 사용한다. `0x450`은 6바이트 in-place packet이며 offset 2의 `0xFAFA` marker가 맞을 때 offset 4 word를 반환한다. 과거 synthetic `0100fafa0010` replay로 Function 0 `0x44c` 도달을 확인했다. Function 0 descriptor offset `0xfe`의 nonzero byte는 handle 유지와 Function 6 `0x44c`/Function `0x0e` `0x458` 도달에 인과적이지만, 실험값 `0x0001`은 실제 driver 응답이 아니며 Function `0x0e` 출력도 미확정이다. 상세 근거는 [3rd Hardlock 분석](analysis/ez2dj3rd-hardlock-function-0e.md)에 둔다 |
| 타이머 소스 | **확인됨** — `timeGetTime` |

---

## 4th Music Select 합성 관측 (2026-09-05)

**확인됨:** 사용자 실행 `20260905-174233-086` frame 1000은 배경, 디스크, ONE/ONE 헤더 그림 순으로 그립니다. 하단·우측 UI는 목적지 곱셈 mask와 가산 그림의 두 pass를 사용합니다. 같은 실행에서 원본이 SRCBLEND=9 / DESTBLEND=6을 요청하지만 기존 HLE가 draw를 거절합니다. **미확정:** 이 거절이 상단 헤더의 빠진 mask에 해당하는지는 실패 로그 상한 때문에 다음 실행으로 확인해야 합니다. [세부 분석](analysis/ez2dj4th-music-select-disc-state.md).

**확정됨:** 후속 실행 `20260905-185621-933`에서는 중앙 mask `texture=250`과 상단 mask `texture=280`의 SRCBLEND=9 / DESTBLEND=6 draw가 성공했고, 사용자 화면에서도 원본과 같은 헤더 가림이 확인되었습니다. 기존 목적지 색상 blend 미지원이 mask draw 전체를 생략한 것이 화면 차이의 원인입니다.

## 3. 갱신 규칙

* 새 사실을 확인하면 같은 작업에서 이 문서와 [EXE_DESIGN.en.md](EXE_DESIGN.en.md)를 함께 갱신한다.
* 주제별 상세 근거는 `docs/analysis/` 아래 문서에 두고 여기서는 결론과 링크만 남긴다.
* 바이트 열 전체를 옮겨 적지 않는다. 구조, 오프셋, 관찰된 동작만 기록한다.
## 2026-09-06 2nd 실행 경계 보정

2nd 실행 로그에서 `GetPrivateProfileIntA` import 부재, legacy I/O helper RVA `0x000782d7`/`0x0007832b`, 그리고 `DirectDrawCreateEx` HLE 연결을 확인했습니다. 따라서 2nd 프로파일은 demo-volume 주입을 사용하지 않으며, D3D IAT 예외는 packer를 보존해야 하는 4th에만 적용합니다. 상세 실행 증거는 [실행 파일 구조 분석](analysis/ez2dj-exe-structures.md)과 [I/O port map](analysis/ez2dj-io-map.md)에 기록합니다.

## 2026-09-10 EZ2Dancer 2nd MOVE 실행 파일 (`ez2d2m`)

**확인됨:** 이 문서가 다루는 첫 비-EZ2DJ 제품이다. `ez2dancer/EZ2Dancer.exe`는 PE32/i386, image base `0x00400000`, entry point RVA `0x00401240`, SizeOfImage `0x0043b000`, timestamp `0x3a5f074c`이며 섹션은 `.text`, `.rdata`, `.data`, `.protect` 넷이다. entry point가 쓰기·실행 `.protect` 안에 있어 EZ2DJ 1st·1st SE·3rd·4th·5th와 같은 자기 수정 packer 계열이다. 문자열에 `\.\FEnteDev`, `\.\HARDLOCK.VXD`, `HLW32Proc`, `API_1LNM.DLL`, `WTSQuerySessionInformationA`가 있어 보호 envelope도 같다.

**확인됨:** packed import directory는 DLL당 stub 하나를 두며 그래픽 진입점이 `DirectDrawCreate`가 아니라 `DirectDrawCreateEx`다. `GetCommandLineA`, `GetWindowsDirectoryA`, `GetPrivateProfileIntA`는 없다. 따라서 `ez2d2m` 프로파일은 세 경계를 끄고 DirectDraw 7 경로를 쓴다.

**추정:** cabinet I/O는 EZ2DJ의 byte 폭 `0x100`~`0x106`이 아니라 16비트 폭 `0x300`~`0x30c`다. 근거는 공개 구현 한 곳뿐이며 원본에서 확인하지 않았다. **미확정:** 이 실행 파일의 raw I/O helper RVA, Hardlock 응답과 descriptor, 실제 실행 성공.

상세 근거는 [ez2d2m CHD 파일시스템과 실행 파일 관찰](analysis/ez2d2m-chd-filesystem.md)과 [EZ2Dancer I/O 포트 맵](analysis/ez2dancer-io-map.md)에 있다.

## 2026-09-11 `ez2d2m` Hardlock transform 두 종류

**확인됨:** 보호 계층은 transform을 두 종류로 보낸다. 1블록 `function=0x000e` 11건과 7블록 `function=0x0011` 1건이다. 앞의 11건은 두 머신에서 값이 같다. `0x0011` 요청은 블록 5만 모든 관찰에서 같고, 블록 0·1·6은 re2DJ runtime 적재 주소를 따라 움직이며 블록 2–4는 머신마다 다르다.

**추정:** 2EZConfig-V2의 상위 계약(사실 대조만 함)에 따르면 `0x000e`는 `API_CRYPT`, `0x0011`은 `API_CODE`다. `API_CODE`는 끝에서 두 번째 블록(블록 5)을 입력으로 한 번 계산해 payload 여러 위치에 쓰므로, 응답이 요청 전체의 함수다. 이에 맞춰 응답 표에 요청 행을 추가했다([설계 247](design/20260911-247-hardlock-payload-response-rows.md)). **미확정:** 유효한 `0x0011` 응답.

## 2026-09-15 `roms/` 미분석 실행 파일 추가 (작업 287)

**확인됨:** 사용자가 제공한 `roms/` 입력에서 구조 분석이 없던 게임 실행 파일을 모두 측정해 [실행 파일 구조 분석](analysis/ez2dj-exe-structures.md)에 6·7·8절로 넣었다. 실행 파일 식별에 크기·MD5·SHA-1·SHA-256을 함께 기록했다.

**확인됨 — 5th.** `roms/ez2dj5th/ez2dj/EZ2DJ.exe`는 1,388,544바이트, PE TimeDateStamp `0x3f53377b`(2003-09-01)이고 진입점이 `.protect`에 있는 보호된 빌드다. 원본 `.idata`는 10 DLL / 161 함수이며 **4th와 DLL 목록·함수 이름 집합이 완전히 같다.** 따라서 5th는 4th 대비 새로운 Win32 API HLE를 요구하지 않는다. packed table도 4th와 같은 36항목 모양이고 DLL당 대표 stub 6개만 다르다.

**확인됨 — 6th.** 6th는 실행 파일이 셋이고 셋 다 **보호 섹션이 없다.** 캐비닛이 실행하는 `EZ2DJ.EXE`(126,976바이트)는 launcher이고, 실제 게임은 자식 `EZ2DJ6th.EXE`(585,728바이트)다. bootstrap의 평문 문자열에는 자식 경로가 둘 있으며, `.\EZ2DJ6TH.EXE` 외에 `.\EZ2DJ1ST\EZ2DJ.EXE`도 있다. 게임 본체의 import는 7 DLL / 137 함수로, 4th·5th 대비 `ADVAPI32`·`AVIFIL32`·`WS2_32` 세 DLL이 전부 빠진다. 즉 6th는 AVI 재생과 Winsock 경계를 요구하지 않는다.

**확인됨 — 6th 동봉 1st Tracks.** `6th.chd`의 `EZ2DJ/Ez2Dj1st/`에 완전한 1st Tracks 배치가 있고, 그 `Ez2DJ.exe`(360,448바이트, `0x411bbf5c`, 2004-08-12)는 **보호되지 않은 재빌드**다. import 6 DLL / 138 함수를 unpack 없이 그대로 읽을 수 있다. `EZ2DJ6th.EXE`와 함께, 게임 표면을 보호 해제 없이 정적으로 읽을 수 있는 유일한 입력이다.

**확인됨 — 1st Tracks.** `roms/ez2dj1st/ez2dj/Ez2DJ.exe`(577,536바이트, `0x3862fd9d`)의 원본 `.idata`는 7 DLL / 141 함수이고, 1st SE 144개의 **진부분집합**이다. 1st SE가 더 가진 것은 `GDI32!BitBlt`, `GDI32!SetBkColor`, `KERNEL32!GetWindowsDirectoryA` 셋뿐이다. `ez2dj`와 `ez2dj1` 두 디렉터리의 실행 파일은 바이트 단위로 같다.

**확인됨 — 1st SE는 두 빌드다.** 디렉터리 덤프(`.gtide` packer, 561,152바이트)와 CHD(`.protect` packer, 634,880바이트)는 PE TimeDateStamp가 같지만 다른 파일이다. 두 빌드의 `.text`·`.rdata`·`.data`·`.reloc`은 배치가 같고 내용이 다르며, `.idata`만 바이트 단위로 같다. packer는 원본 import table 섹션을 건드리지 않고 본체만 변환한다.

**미확정:** 5th 디렉터리 배치가 `ez2dj5.chd`에서 나왔는지, 6th bootstrap이 두 자식 중 어느 쪽을 언제 고르는지, 6th `EZ2DJ.INI`가 평문이 아닌 이유와 소비 경로, 그리고 새로 확인한 세 제품의 Hardlock 응답 계약.

## 2026-09-17 보호 빌드 복호화와 3rd 게임 상태 (작업 291~297)

근거 분석: [보호 빌드의 런타임 복호화](analysis/protected-build-runtime-decryption.md), [I/O 포트 맵](analysis/ez2dj-io-map.md), [3rd 프레임 pacing](analysis/ez2dj3rd-frame-pacing.md), [3rd 데모 플레이와 설정 레지스트리](analysis/ez2dj3rd-demo-play.md)

**확인됨 — `.protect` 빌드의 진입점 breakpoint는 복호화 이전이다.** 1st Tracks·1st SE(CHD)·3rd·4th·5th·`ez2d2m` 여섯 빌드에서 진입점 복원 직후 덤프에는 원본 문자열과 port helper가 0건이고, 게스트가 몇 초 실행된 뒤의 덤프에는 나타난다. 3rd는 두 덤프가 이미지의 36.3%에서 다르다. 정적 분석에는 재개 후 덤프(`--image-dump`의 `resumed`)를 쓴다.

**확인됨 — legacy I/O helper RVA는 각 빌드 자신의 값이다.** 컴파일러 런타임 `inp`/`outp` 묶음의 바이트 시그니처로 재개 후 덤프를 탐색한 결과, 바이트 폭 다섯 빌드의 `inportb`·`outportb`가 프로파일 값과 정확히 일치한다. 묶음의 출력 쪽 간격은 1st·1st SE와 3rd·4th·5th가 다르다. `ez2d2m`은 port I/O를 게임 코드에 인라인해 시그니처 대상이 아니다.

**확인됨 — 3rd는 MSVC 디버그 구성이다.** 지역 변수 `0xcccccccc` 채움, 스택 검사, incremental-link thunk가 있어 함수 경계와 호출 대상이 정적으로 드러난다.

**확인됨 — 3rd의 INI는 자체 파서이고 설정은 키-주소 레지스트리다.** 섹션 없는 따옴표 키 형식이며, 설정 객체 `[0x004edbc8]`에 묶인 키는 INI의 25개 키 중 24개이고, 나머지 `UseIOCard`는 코드가 참조하지 않는다. `AutoPlay` 키는 없다. (작업 300에서 "25개와 정확히 같다"를 정정)

**확인됨 — 3rd의 데모와 autoplay는 별개 플래그다.** 데모 시작 루틴 `0x0048aa31`은 게임 장면을 동기 실행하며 데모 플래그 `[0x00a2946c]`(오버레이·입력 해제·음소거·입력 시 종료)와 autoplay 플래그 `[0x00a29508]`를 짝으로 세운다. autoplay 플래그만 1로 쓰면 데모 부작용 없이 게임이 노트를 치며, 값은 **곡 시작 때 고정**된다. 입력 매니저는 매 프레임 슬롯 `0x1b`로 이 값을 뒤집는다. 슬롯 `0x1b`의 물리 바인딩은 **미확정**이다.

**확인됨 — 3rd의 프레임 루프는 `목표 - 경과` sleep 피드백이다.** 목표 주기는 약 17 ms로 **추정**되며 원본 상수는 **미확정**이다. 현재 프레임률은 정적 링크된 SDL3의 기본 `timeBeginPeriod(1)`에 의존한다.

**확인됨 — 3rd는 마우스 커서를 숨긴다.** `ShowCursor`·`SetCursor`를 import 한다. re2DJ는 창 경계에서 커서를 되살린다.

## 2026-09-18 4th 데모와 autoplay 플래그 (작업 300)

근거 분석: [4th 데모 플레이와 autoplay 플래그](analysis/ez2dj4th-demo-play.md)

**확인됨 — 4th는 3rd와 같은 구조다.** TimeDateStamp `0x3d369bfd` 빌드에서 설정 레지스트리 `[0x005111e0]`(24개 키, `AutoPlay` 없음), 데모 플래그 `[0x00ac290c]`, 데모 시작 루틴 `0x004a4430`, autoplay 플래그 `[0x00ac29b0]`(setter `0x00437600`, getter `0x004375f0`)가 3rd와 같은 방식으로 연결된다. 어트랙트에서 두 플래그가 데모 구간에만 함께 1이 되고, OSD로 autoplay를 켜 곡이 자동 연주됨을 사용자가 확인했다. 노트 판정 계열 getter 호출처가 두 벌이어서 게임 모드가 둘인 것으로 **추정**한다.

## 2026-09-18 5th 데모와 autoplay 플래그 (작업 301)

근거 분석: [5th 데모 플레이와 autoplay 플래그](analysis/ez2dj5th-demo-play.md)

**확인됨 — 5th도 3rd·4th와 같은 구조다.** TimeDateStamp `0x3f53377b` 빌드에서 설정 레지스트리 `[0x00516350]`(26개 키, `AutoPlay` 없음), 데모 플래그 `[0x00aee194]`, 데모 시작 루틴 `0x004aabf7`, autoplay 플래그 `[0x00aee238]`(setter `0x00437790`, getter `0x00437780`)가 같은 방식으로 연결된다. 어트랙트에서 두 플래그가 데모 구간에만 함께 1이 되고, OSD로 autoplay를 켜 곡이 자동 연주됨을 사용자가 확인했다.

**추정 — 5th는 부분 자동 설정을 새로 갖는다.** 4th에 없던 `AutoScratch`·`AutoPedal` 키가 레지스트리에 묶이며, 첫 판정 계열의 노트 데이터 필드가 12바이트 뒤(`+0x1d8`)로 밀렸다. 실제 동작은 미확정이다.

## 2026-09-18 1st SE 장면 엔진과 autoplay 플래그 (작업 302)

근거 분석: [1st SE 자동 플레이 장면과 autoplay 플래그](analysis/ez2dj1stse-demo-play.md)

**확인됨 — 1st SE CHD 빌드는 장면 엔진이다.** TimeDateStamp `0x3862df27` 빌드는 등록 함수 `0x00423670`으로 60개 장면을 등록하며, 데모는 전용 장면 `DemoGame`·`ClubMixDemoGame`, 오버레이는 `ShowDemoPlay` 장면이다. 3rd~5th의 "데모 플래그 + 일반 게임 장면" 구조와 다르다.

**확인됨 — autoplay 플래그는 `[0x01c3f3a4]`이다.** 스스로 플레이하는 세 장면(`DemoGame`, `ClubMixDemoGame`, `HowToPlayGame`)이 초기화 콜백에서 1, 종료 콜백에서 0으로 쓰고, 플레이어 장면들이 자동·수동 분기와 입력 처리에 읽으며, 곡 재생기 시작에 인자로 넘긴다. 어트랙트 읽기 폴링과 사용자의 OSD 확인으로 동작을 확인했다.

**확인됨 — 효과음 버퍼 생성 플래그는 `0x000140e2`(`DSBCAPS_STATIC | GETCURRENTPOSITION2` 등)이다.** 현재 DirectSound HLE가 이를 스트리밍으로 분류하는 것은 원본 구조가 아니라 re2DJ 쪽 문제다.

## 2026-09-18 1st Tracks 데모 전용 플레이어 장면 (작업 304)

근거 분석: [1st Tracks 자동 플레이 장면](analysis/ez2dj1st-demo-play.md)

**확인됨 — 1st Tracks도 장면 엔진이지만 데모 전용 플레이어 장면이 있다.** TimeDateStamp `0x3862fd9d` 빌드는 등록 함수 `0x00424280`으로 53개 장면을 등록하고, 일반 `player`와 별도로 `DemoPlayer`·`ClubMixDemoPlayer`를 둔다. 곡 재생기 `0x0041af60`에 데모 장면만 상수 1을 넘기며, 그 값이 저장되는 `[0x0055bc4c]`는 곡 진행 모듈만 읽는다. 곡 도중 1로 써도 변화가 없었다.

**추정 — 1st Tracks에는 전환 가능한 autoplay 변수가 없다.** 자동 연주는 데모 플레이어 장면의 복제된 코드에 있다고 보며, `game_controls`로 표현하지 않는다.

## 2026-09-18 EZ2Dancer 2nd MOVE 데모와 autoplay 플래그 (작업 305)

근거 분석: [EZ2Dancer 2nd MOVE 데모와 autoplay 플래그](analysis/ez2d2m-demo-play.md)

**확인됨 — `ez2d2m`은 클래스 쌍 구조다.** TimeDateStamp `0x3a5f074c` 빌드는 장면 등록 함수 없이 `NormalGame`·`DemoGame`처럼 클래스마다 `OnCreateGame`·`OnDestroyGame`과 Director를 두고, 각 함수가 자기 이름 문자열을 로그 함수에 넘긴다.

**확인됨 — autoplay 플래그는 `[0x007fa424]`다.** `NormalGame::OnCreateGame`이 이 값이 1일 때만 채널 3~`0x12`를 자동으로 설정하고, 데모는 같은 값을 상수로 쓴다. 게임에 내장된 토글이 입력 슬롯 `0xc`로 이 값을 뒤집는다. 데모가 이 플래그를 쓰지 않으므로 어트랙트 폴링으로는 확인할 수 없고, OSD 토글로 자동 연주를 확인했다.
