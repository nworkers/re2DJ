# 릴리즈 노트 / Release Notes

## v0.0.57 (2026-09-30)

### 한국어

Windows command prompt용 빌드 bat를 Win32 Debug용과 Release용 두 개로 정리했습니다. Release 스크립트가 테스트 단계에서 항상 실패하던 문제도 고쳤습니다.

#### 1. 빌드 bat 정리
- `scripts\build_win32_debug.bat`: Win32 Debug를 configure하고 빌드합니다. 결과물은 `build\windows-x86\bin\Debug`에 생깁니다.
- `scripts\build_win32_release.bat`: Win32 Release를 configure하고 빌드한 뒤 CTest를 실행합니다. `-SkipTests`를 주면 테스트를 건너뜁니다. 결과물은 `build\windows-x86\bin\Release`에 생깁니다.
- 기존 `build_win32.bat`와 `build_release.bat`는 이 두 파일로 대체했습니다. 둘 다 어느 작업 디렉터리에서나 실행할 수 있습니다.

#### 2. 빌드 디렉터리 수정
- `build.ps1`과 `build_release.ps1`은 빌드 디렉터리를 `build\<preset 이름>`으로 가정했습니다. 그런데 `windows-x86-debug` preset은 `build\windows-x86`에 빌드합니다. 그래서 Release CTest 단계가 없는 디렉터리를 찾다가 실패했고, 두 스크립트가 안내하는 결과물 경로도 틀렸습니다.
- 이제 두 스크립트는 `CMakePresets.json`에서 preset의 `binaryDir`을 읽습니다.

#### 3. 검증
- 두 bat를 저장소 밖 디렉터리에서 실행했습니다. Debug 빌드가 성공했고, Release는 빌드와 CTest 6개가 모두 통과했습니다.

---

### English

The Windows command-prompt build batch files are now two: one for Win32 Debug and one for Win32 Release. A bug that always failed the Release script at its test step is fixed.

#### 1. Build batch files
- `scripts\build_win32_debug.bat` configures and builds Win32 Debug into `build\windows-x86\bin\Debug`.
- `scripts\build_win32_release.bat` configures and builds Win32 Release into `build\windows-x86\bin\Release`, then runs CTest. `-SkipTests` skips the tests.
- They replace `build_win32.bat` and `build_release.bat`. Both run from any working directory.

#### 2. Build directory fix
- `build.ps1` and `build_release.ps1` assumed the build directory was `build\<preset name>`, but the `windows-x86-debug` preset builds into `build\windows-x86`. The Release CTest step therefore looked for a directory that did not exist and failed, and both scripts printed the wrong output path.
- Both scripts now read the preset's `binaryDir` from `CMakePresets.json`.

#### 3. Validation
- Both batch files were run from a directory outside the repository. The Debug build succeeded, and Release built and passed all 6 CTest tests.

---

## v0.0.56 (2026-09-29)

### 한국어

EZ2Dancer 2nd MOVE(ez2d2m)가 Linux x86·x64에서 실행됩니다. 타이틀 화면을 거쳐, 코인을 넣으면 곡 선택과 플레이까지 진행합니다. EZ2Dancer 보드의 키 배치는 Windows와 Linux가 함께 쓰는 공용 core로 옮겼습니다. 새 동작은 Windows 11에서 측정한 값을 따릅니다.

#### 1. Linux ez2d2m 실행 (작업 427)
- **창**: `CreateWindowExA`가 `WS_BORDER`를 받습니다. `DefWindowProcA`의 `WM_NCCALCSIZE`는 측정대로 테두리 창의 사각형을 사방 1픽셀씩 줄입니다.
- **예외 보고**: `UnhandledExceptionFilter`가 처리되지 않은 게스트 예외의 코드·주소·인자를 실행 결과에 남기고 멈춥니다.
- **EZ2Dancer 보드**: Linux IO trap이 word 폭 보드에 답합니다. 전에는 word 폭 보드가 설정되면 trap이 꺼졌습니다. 키 배치 표는 공용 `ez2dancer_keyboard_map`으로 옮겨 Windows 입력 코드도 같이 씁니다. 게임은 입력 helper 두 곳(`0xb169`, `0xb4cb`)에서 보드를 읽기 때문에, 읽기는 opcode로 판정합니다.

#### 2. 열리지 않은 COM1 (작업 428)
- ez2d2m은 `COM1`을 열지 못해도 그 무효 핸들로 계속 씁니다. 코인을 넣을 때마다 overlapped `WriteFile`을 부르므로, Linux에서는 코인을 넣는 순간 멈췄습니다.
- 무효 핸들에 대한 다음 호출은 Windows 11 측정대로 실패합니다.
  - overlapped `ReadFile`·`WriteFile`
  - 시리얼 함수 8개(`SetCommState`, `GetCommState`, `SetCommTimeouts`, `PurgeComm`, `SetupComm`, `SetCommMask`, `ClearCommError`, `WaitCommEvent`)
  - `GetOverlappedResult`
- `CloseHandle(INVALID_HANDLE_VALUE)`는 성공합니다.

#### 3. Windows에 영향을 주는 변경
- EZ2Dancer 키 배치를 공용 표에서 읽습니다. 기본 키는 바뀌지 않았습니다.
- 작업 427은 코인이 오르지 않는다고 기록했지만, 키가 전달되지 않아 생긴 오판이었습니다. 코인은 두 host 모두에서 크레딧을 올립니다.

#### 4. 검증
- Windows x86 CTest 6개와 Linux x64·x86 CTest 4개가 통과합니다. 단위 검사는 5529 / 5526 checks입니다.
- Linux ez2d2m은 두 폭에서 코인을 넣은 뒤에도 시간 제한까지 멈추지 않았습니다. x64는 곡 선택 화면까지 진행했습니다.

---

### English

EZ2Dancer 2nd MOVE (ez2d2m) runs on Linux x86 and x64. It passes its title screen and, with a coin in, goes on to music select and play. The EZ2Dancer board's key bindings moved into a shared core used by both Windows and Linux. New behaviour follows values measured on Windows 11.

#### 1. Running ez2d2m on Linux (task 427)
- **Window**: `CreateWindowExA` takes `WS_BORDER`. As measured, `DefWindowProcA`'s `WM_NCCALCSIZE` insets a bordered window's rectangle by one pixel on each side.
- **Exception report**: `UnhandledExceptionFilter` records the code, address, and parameters of an unhandled guest exception in the run result and stops.
- **EZ2Dancer board**: The Linux IO trap answers the word-wide board; before, the trap turned itself off for a word-wide board. The key-binding table moved into the shared `ez2dancer_keyboard_map`, which the Windows input code uses too. The game reads the board through two input helpers (`0xb169` and `0xb4cb`), so reads are recognised by opcode.

#### 2. The COM1 port that does not open (task 428)
- ez2d2m keeps writing through its `COM1` handle even when the port failed to open. It calls overlapped `WriteFile` for every coin, so on Linux it stopped the moment a coin went in.
- These calls on an invalid handle now fail as measured on Windows 11:
  - overlapped `ReadFile` and `WriteFile`;
  - the eight serial functions (`SetCommState`, `GetCommState`, `SetCommTimeouts`, `PurgeComm`, `SetupComm`, `SetCommMask`, `ClearCommError`, `WaitCommEvent`);
  - `GetOverlappedResult`.
- `CloseHandle(INVALID_HANDLE_VALUE)` succeeds.

#### 3. Changes that reach Windows
- EZ2Dancer key bindings are read from the shared table; the default keys are unchanged.
- Task 427 recorded that a coin did not raise the credit count. That was a misreading, because the key had not reached the game; a coin raises the count on both hosts.

#### 4. Validation
- All 6 Windows x86 CTest tests and all 4 Linux x64/x86 CTest tests pass (5529 / 5526 unit checks).
- Linux ez2d2m ran on both widths without stopping until the timeout, even after coins went in; x64 got as far as music select.

---

## v0.0.55 (2026-09-29)

### 한국어

Linux x86·x64에서 EZ2DJ 4th, 1st SE, 5th가 창을 닫을 때까지 돌아갑니다. 화면, 소리, 키보드·마우스 입력, IO 보드가 모두 연결됐습니다. 1st도 DirectX 6 초기화와 소리 스레드까지 진행합니다. 새로 만든 규칙은 대부분 Windows와 Linux가 함께 쓰는 공용 core에 두었고, Windows 11에서 측정한 값을 따릅니다.

#### 1. Linux 4th를 끝까지 (작업 384~403)
- **입력**: DirectInput core를 공용으로 옮기고 Linux `dinput.dll`을 추가했습니다. IO 보드 포트 판정도 공용 trap core로 옮겨, Linux 두 폭의 signal handler가 답합니다. SDL의 키·마우스·커서가 `GetAsyncKeyState`, DirectInput, `GetCursorPos`, IO 보드로 들어갑니다.
- **소리**: winmm mixer 하나를 모델링하고(Windows 11 host 측정), DirectSound 버퍼 제어를 core로 옮겼습니다. Linux는 SDL3_mixer로 소리를 냅니다(`--audio-gain-db`).
- **화면**: 그리기 규칙(draw 계획, 고정 기능 상태, 변환, fade)을 `direct3d_draw.h` core로 옮겼습니다. Linux 창은 공용 SDL3/OpenGL backend로 그립니다. 기본 2배 크기, Alt+1/2/3, 더블클릭 전체 화면, 제목 FPS, `--fullscreen`/`--windowed`를 지원합니다.
- **표면과 GDI**: 표면 DC, `StretchDIBits`, `EnumSurfaces`/`RestoreAllSurfaces`, DX7 vertex buffer, `CreateSolidBrush`·`FillRect`·`SetTextColor`·`SetBkMode`·`DrawTextA`(GNU Unifont 8×16 ASCII 글리프, OFL 1.1)를 추가했습니다.
- **파일과 메시지**: 현재 디렉터리, `GetFileType`, `FindFirstFileA`(제품 VFS 목록 규칙 공유), `GetFileAttributesA`를 추가했습니다. `PeekMessageA`/`DispatchMessageA`와 메시지 큐(WM_PAINT, WM_TIMER)도 구현했습니다.
- **실행**: 호출 한도 없이 창을 닫을 때까지 실행합니다. 호출 한도는 `--call-limit`로만 겁니다.

#### 2. Linux 1st (작업 404~419)
- **게스트 예외**: 게스트 예외를 게스트의 SEH 체인으로 넘기고 `RtlUnwind`를 구현했습니다. Linux x86에서 게스트가 `%gs`를 바꿔 생기던 coredump도 고쳤습니다.
- **kernel32·user32**: CRT 시작 함수(critical section, TLS, Interlocked, `IsBadReadPtr`), `ShowWindow`, `EnumDisplaySettingsA`, `Sleep`, `HeapValidate`를 추가했습니다. `GetPrivateProfileIntA`/`StringA`/`SectionNamesA`는 공용 INI core로 처리합니다. `wsprintfA`는 가변 인자를 읽어 측정한 서식 규칙으로 처리합니다.
- **DirectX 6**: `DirectDrawEnumerateA`, `DirectDrawCreate`, `IDirectDraw4`, `IDirect3D3`, `IDirectDrawSurface4`, `IDirect3DDevice3`, `IDirect3DViewport3`를 추가했습니다. `FindDevice`, Z 형식, `GetCaps`, `D3DVIEWPORT2` 변환은 Windows DX6 facade와 공용 core를 함께 씁니다.
- **게스트 스레드**: `CreateThread`로 만든 게스트 스레드가 host 스레드에서 돕니다. 게스트 코드와 HLE는 게스트 잠금 하나로 한 번에 한 스레드만 실행하고, 잠금은 import 안에서만 넘어갑니다. x64에서는 transition 상태를 스레드끼리 넘깁니다. 메인이 아닌 스레드의 fault나 `ExitProcess`는 프로세스를 끝냅니다. `SetThreadPriority`, 스레드 핸들 대기, 스레드별 ID·last error도 들어갔습니다.

#### 3. Linux 1st SE와 5th (작업 420~426)
- **1st SE**: CHD로 실행합니다. BMP 파일 읽기(`LoadImageA`와 GDI 비트맵), DX6 texture(`IDirect3DTexture2`), `Blt`/`BltFast`, DX6 vertex buffer를 추가했습니다.
- **소프트웨어 페이싱**: vsync를 요청해도 swap이 막지 않는 host(WSLg)에서는 공용 backend가 화면 주기에 맞춰 present를 기다립니다. 1st SE가 112 FPS 대신 60 FPS로 돕니다. vsync가 실제로 막는 Windows에서는 켜지지 않습니다.
- **표면 Lock**: `Lock`/`Unlock` 규칙을 공용 core(`PlanLock`)로 옮겼습니다. 화면에 내보내는 렌더 타깃을 Lock하면 GL 그림을 되읽고, Unlock하면 다시 올립니다. 4th의 F1(TEST) 테스트 모드 메뉴가 Linux와 Windows 모두에서 보입니다. 전에는 두 host 모두 검은 화면이었습니다.
- **5th**: `StretchDIBits`가 8비트 팔레트 DIB를 받습니다(측정). 5th가 Linux 두 폭에서 창을 닫을 때까지 돕니다.

#### 4. Windows에 영향을 주는 변경
- 4th의 F1 테스트 모드 화면이 나옵니다(작업 425).
- `GetFileAttributesA`를 게스트 이미지 기준으로 답합니다. 전에는 host 현재 디렉터리 기준이었습니다(작업 403).
- 공용 core로 옮긴 규칙은 각 작업에서 Windows 실제 실행 기록을 변경 전후로 비교해, 같게 유지됐음을 확인했습니다.

#### 5. 검증
- Windows x86 CTest 6개, Linux x64·x86 CTest 4개가 통과합니다(단위 5360 / 5357 checks). Linux in-process probe에 게스트 스레드 합성 검사가 포함됩니다.
- Linux 4th·1st SE·5th는 두 폭에서 시간 제한까지 멈추지 않았습니다. 1st는 두 폭에서 `user32!LoadImageA`까지 진행합니다.

---

### English

On Linux x86 and x64, EZ2DJ 4th, 1st SE, and 5th now run until their window is closed, with picture, sound, keyboard and mouse input, and the IO board all connected. 1st gets through DirectX 6 initialization and its sound thread. Most new rules live in shared cores used by both Windows and Linux and follow values measured on Windows 11.

#### 1. Linux 4th end to end (tasks 384–403)
- **Input**: The DirectInput core is shared and Linux gains `dinput.dll`. IO-board port decisions moved into a shared trap core, answered by both Linux widths' signal handlers. SDL keys, mouse, and cursor reach `GetAsyncKeyState`, DirectInput, `GetCursorPos`, and the IO board.
- **Sound**: One winmm mixer is modelled (measured on a Windows 11 host), DirectSound buffer controls moved into the core, and Linux plays sound through SDL3_mixer (`--audio-gain-db`).
- **Picture**: The drawing rules (draw plan, fixed-function state, transforms, fade) moved into the `direct3d_draw.h` core. The Linux window draws through the shared SDL3/OpenGL backend, with a default 2x scale, Alt+1/2/3, double-click fullscreen, the title FPS, and `--fullscreen`/`--windowed`.
- **Surfaces and GDI**: Surface DCs, `StretchDIBits`, `EnumSurfaces`/`RestoreAllSurfaces`, DX7 vertex buffers, and `CreateSolidBrush`, `FillRect`, `SetTextColor`, `SetBkMode`, and `DrawTextA` (GNU Unifont 8×16 ASCII glyphs, OFL 1.1).
- **Files and messages**: The current directory, `GetFileType`, `FindFirstFileA` (sharing the product VFS listing rules), and `GetFileAttributesA`; `PeekMessageA`/`DispatchMessageA` with a message queue (WM_PAINT, WM_TIMER).
- **Running**: A run goes on until the window is closed; a call limit applies only with `--call-limit`.

#### 2. Linux 1st (tasks 404–419)
- **Guest exceptions**: Guest exceptions are delivered to the guest's SEH chain, and `RtlUnwind` is implemented. A Linux x86 coredump caused by the guest changing `%gs` is fixed.
- **kernel32 and user32**: CRT start-up functions (critical sections, TLS, Interlocked, `IsBadReadPtr`), `ShowWindow`, `EnumDisplaySettingsA`, `Sleep`, and `HeapValidate`. `GetPrivateProfileIntA`/`StringA`/`SectionNamesA` go through a shared INI core. `wsprintfA` reads its variadic arguments and follows the measured formatting rules.
- **DirectX 6**: `DirectDrawEnumerateA`, `DirectDrawCreate`, `IDirectDraw4`, `IDirect3D3`, `IDirectDrawSurface4`, `IDirect3DDevice3`, and `IDirect3DViewport3`. `FindDevice`, the depth format, `GetCaps`, and the `D3DVIEWPORT2` transform share a core with the Windows DX6 facade.
- **Guest threads**: Guest threads from `CreateThread` run on host threads of their own. One guest lock lets only one thread run guest code or the HLE at a time, and it changes hands only inside imports. On x64 the transition state is handed between threads. A fault or `ExitProcess` in a thread other than the main one ends the process. `SetThreadPriority`, waits on thread handles, and per-thread IDs and last errors are included.

#### 3. Linux 1st SE and 5th (tasks 420–426)
- **1st SE**: It runs from its CHD. Bitmap files (`LoadImageA` with GDI bitmaps), DX6 textures (`IDirect3DTexture2`), `Blt`/`BltFast`, and DX6 vertex buffers are added.
- **Software pacing**: On a host whose swap does not block despite vsync (WSLg), the shared backend waits out the display period after each present, so 1st SE runs at 60 FPS instead of 112. It stays off on Windows, where vsync does block.
- **Surface Lock**: The `Lock`/`Unlock` rules moved into a shared core (`PlanLock`). Locking the render target that is presented reads the GL picture back, and unlocking writes it back, so 4th's F1 (TEST) test-mode menu shows on both Linux and Windows; before, both hosts showed a black screen.
- **5th**: `StretchDIBits` takes 8-bit palettized DIBs (measured), and 5th runs on both Linux widths until its window is closed.

#### 4. Changes that reach Windows
- 4th's F1 test-mode screen now shows (task 425).
- `GetFileAttributesA` answers from the guest image; it used to query relative to the host's current directory (task 403).
- For each rule moved into a shared core, the task compared real Windows run records before and after the change and confirmed they stayed the same.

#### 5. Validation
- All 6 Windows x86 CTest tests and all 4 Linux x64/x86 CTest tests pass (5360 / 5357 unit checks); the Linux in-process probe includes synthetic guest-thread checks.
- Linux 4th, 1st SE, and 5th ran on both widths without stopping until the timeout; 1st gets as far as `user32!LoadImageA` on both widths.

---

## v0.0.54 (2026-09-26)

### 한국어

Linux에서 실제 4th가 DirectDraw 표면을 만들고 Direct3D 장치를 설정한 뒤, 글꼴 파일을 읽고 DirectSound를 초기화합니다. 두 폭 모두 API 호출 1,872번 뒤 `dinput.dll!DirectInputCreateA`에서 멈춥니다. 화면은 아직 검은색입니다. 그리기와 화면 표시는 이후 단계에서 다룹니다. DirectX core에 표면과 장치를 옮겼고, DirectSound에도 Windows와 Linux가 함께 쓰는 core를 만들었습니다.

#### 1. DirectX core 3·4단계 (작업 381·382)
- **표면**: `CreateSurface` 규칙(flip 주 표면과 back buffer, depth, RGB565 texture, offscreen), pitch, 표면 설명, attach를 core로 옮겼습니다. Linux `IDirectDrawSurface7`은 픽셀을 게스트 메모리에 둡니다. 게스트 COM 객체는 이제 다른 facade 객체의 참조와 게스트 자원을 갖고, 사라질 때 함께 돌려줍니다.
- **장치**: `IDirect3D7::CreateDevice` 규칙과 장치 상태를 core로 옮겼습니다. 장치 상태는 초기값, render·texture stage state, transform, 장면, viewport입니다. Linux `IDirect3DDevice7`은 게임의 장치 설정 호출을 모두 처리합니다. 그 순서와 값은 Windows 기록과 같습니다.
- **SDK 검사**: Windows `static_assert`가 core 상수 네 개를 바로잡았습니다. 대상은 `DDCAPS2_NOPAGELOCKREQUIRED`, `DDERR_CANNOTATTACHSURFACE`, `D3DERR_SCENE_IN_SCENE`, `D3DERR_SCENE_NOT_IN_SCENE`입니다.

#### 2. DirectSound와 창 조회 (작업 383)
- **DirectSound core**: `re2dj::audio`에 게스트 ABI와 버퍼 생성·caps·복제·lock 분할 규칙을 두었습니다. Windows facade와 `LegacyAudioBuffer`가 이 core를 씁니다.
- **Linux `dsound.dll`**: `DirectSoundCreate`, `IDirectSound` 전체, `IDirectSoundBuffer`의 생성·caps·형식·lock·unlock을 구현했습니다. sample은 게스트 메모리에 두고, 복제본은 원본과 sample을 공유합니다. Linux의 소리 출력은 아직 없습니다.
- **user32**: `GetForegroundWindow`를 구현했습니다. `GetWindowLongA`는 Windows 11에서 측정한 last error 규칙을 따릅니다.

#### 3. 검증
- Windows 실제 4th의 그래픽 기록과 오디오 생성·복제 기록은 각 작업의 변경 전후가 같습니다.
- Linux x86과 x64의 실행 기록은 주소만 맞추면 같습니다.

---

### English

On Linux the real 4th now creates its DirectDraw surfaces, sets up its Direct3D device, reads its font files, and initializes DirectSound, stopping at `dinput.dll!DirectInputCreateA` after 1,872 API calls on both widths. The screen is still black; drawing and presentation come in a later phase. Surfaces and the device moved into the DirectX core, and DirectSound gets a core of its own shared by Windows and Linux.

#### 1. DirectX core phases 3 and 4 (tasks 381–382)
- **Surfaces**: The `CreateSurface` rules (a flipping primary with its back buffer, depth, RGB565 textures, offscreen), the pitch, surface descriptions, and attachments moved into the core. Linux's `IDirectDrawSurface7` keeps its pixels in guest memory. Guest COM objects can now hold references to other facade objects and guest resources, and return them when they go.
- **Device**: The `IDirect3D7::CreateDevice` rules and the device state moved into the core. The device state covers the initial values, render and texture stage states, transforms, scenes, and the viewport. Linux's `IDirect3DDevice7` handles all of the game's device setup calls, in the same order and with the same values as the Windows record.
- **SDK checks**: Windows `static_assert`s corrected four core constants: `DDCAPS2_NOPAGELOCKREQUIRED`, `DDERR_CANNOTATTACHSURFACE`, `D3DERR_SCENE_IN_SCENE`, and `D3DERR_SCENE_NOT_IN_SCENE`.

#### 2. DirectSound and window queries (task 383)
- **DirectSound core**: `re2dj::audio` holds the guest ABI and the rules for buffer creation, caps, duplication, and how a lock divides a buffer. The Windows facade and `LegacyAudioBuffer` use this core.
- **Linux `dsound.dll`**: `DirectSoundCreate`, all of `IDirectSound`, and `IDirectSoundBuffer`'s creation, caps, format, lock, and unlock. Samples live in guest memory, and duplicates share their source's samples. Linux has no sound output yet.
- **user32**: `GetForegroundWindow` is implemented, and `GetWindowLongA` follows the last-error rules measured on Windows 11.

#### 3. Validation
- On Windows, the real 4th's graphics record and audio creation and duplication records match the pre-change build for each task.
- Linux x86 and x64 produce the same run record once addresses are normalized.

---

## v0.0.53 (2026-09-26)

### 한국어

Linux에서 실제 4th가 보호 envelope을 지나 원본 CRT와 WinMain으로 들어가고, 게임 창을 만든 뒤 DirectDraw 초기화까지 진행합니다. 두 폭 모두 API 호출 1,824번 뒤 `IDirectDraw7::CreateSurface`에서 멈추고, 호스트 화면에는 게임 창이 뜹니다. Windows와 Linux가 함께 쓰는 DirectX 공용 core도 이 릴리즈에서 시작합니다.

#### 1. Hardlock과 보호 envelope (작업 360~364, 367)
- **Hardlock HLE 공용화**: 설정 조립, `DeviceIoControl` 완료 규칙, 장치 경로 판정을 공용 core로 옮겼습니다. Windows 실제 4th의 Hardlock 기록은 변경 전후가 같습니다. WTS class 4는 `WTSSessionId`로 바로잡았습니다.
- **게스트 장치와 process 환경**: Linux `kernel32` facade가 `\\.\FEnteDev`를 열고 `DeviceIoControl`을 처리합니다. `GuestProcess`는 게스트의 ID, error mode, heap, image·`VirtualAlloc` 영역의 page 보호 기록을 갖습니다. `advapi32`, `wtsapi32` facade도 추가했습니다.
- **envelope 두 번째 층**: 원본 import 표 재구성, `ExitProcess` hook, `Read/WriteProcessMemory`, thread timer를 처리해 원본 진입점까지 갑니다. 복호화 루프의 descriptor·transform 수가 Windows와 같습니다.

#### 2. 원본 CRT에서 WinMain까지 (작업 368~371)
- **MSVC CRT 시작**: heap, 시작 정보, 명령줄·환경, code page 949(실측), module 경로를 제공합니다. stop stub은 게스트 SEH에서 제외했습니다.
- **정적 초기화**: 이름 없는 event, host 시계와 시간 export, `GetTimeZoneInformation`을 구현했습니다. 게임 자신의 Hardlock 로그인도 통과합니다.
- **게스트 파일과 winmm**: CHD와 overlay(copy-on-write)로 게스트 파일을 제공하고(`EZ2DJ.ini` 등), `timeBeginPeriod`/`timeGetTime`을 구현했습니다.
- **API 호출 기록**: facade 호출마다 게스트에서 읽은 입력, 게스트에 쓴 출력, last error, 반환값을 `logs/re2dj-<시각>.api.log`에 남깁니다. Hardlock buffer는 길이만 남깁니다.

#### 3. 창과 DirectDraw (작업 372~374, 377, 380)
- **게스트 호출**: HLE handler가 window procedure 같은 게스트 함수를 끝까지 실행할 수 있습니다. x86은 직접 부르고, x64는 중첩 compat-mode 전환을 씁니다. 게스트가 그 사이에 부르는 API는 중첩 호출로 처리되며, API log에 들여써서 남습니다.
- **창 생성**: `RegisterClassA`, `CreateWindowExA`, `DefWindowProcA`, `UpdateWindow`와 `gdi32!GetStockObject`를 구현했습니다. 창 생성 메시지 순서와 인자는 Windows 11에서 측정한 값을 따릅니다.
- **DirectDraw 진입**: facade COM 객체(vtable은 `"<인터페이스>::<메서드>"` export)를 도입했습니다. `DirectDrawEnumerateExA`(모니터 하나), `DirectDrawCreateEx`, `IDirectDraw7`, `IDirect3D7`의 열거와 caps, `SetCooperativeLevel`, `SetDisplayMode`가 동작합니다.
- **DirectX 공용 core (`re2dj_directx`)**: 32비트 게스트 ABI 구조체, caps, 장치·형식·표시 모드 열거, 협조 수준·표시 모드 규칙을 Windows COM facade와 Linux gate facade가 함께 씁니다. Windows는 SDK와의 구조·상수 일치를 `static_assert`로 검사하며, 실제 4th의 DirectX 기록은 변경 전후가 같습니다.
- **Linux 호스트 창**: 게임이 `SetCooperativeLevel`을 부를 때 SDL3/OpenGL 창(640×480)이 뜹니다. 아직 그리는 것이 없어 검은 화면입니다. `--hold-window`를 주면 실행이 멈춘 뒤에도 창이 남습니다.

#### 4. 제품 표시와 로깅 (작업 365·366, 375·376)
- **버전 머리말**: 창 제목, OSD, `--version`, `--help`, 실행 로그, 진단 도구가 모두 `re2DJ v0.0.53 (Win/x86 Debug)`처럼 OS·아키텍처·빌드 형식을 함께 보여 줍니다. 창 제목은 두 플랫폼이 같은 함수로 만듭니다.
- **spdlog**: CLI 실행 출력과 Windows injected runtime 로그를 spdlog로 옮겼습니다.

#### 5. 구조 정리 (작업 378·379)
- **native helper IPC 제거**: Linux i386 helper와 `--linux-helper`, Windows native helper(선택 빌드), helper protocol, 관련 preset과 script를 지웠습니다. Linux는 두 폭 모두 in-process로만 실행합니다.
- **플랫폼 경계**: Linux 코드와 target이 `src/platform/windows`의 파일을 참조하지 않도록 정리했습니다. 공용 probe fixture는 `src/platform/native_probe_fixture`로 옮겼습니다.

#### 6. 기타
- **수정**: Windows `re2dj_windows_vfs_runtime_probe`의 crash를 고쳤습니다(작업 362). v0.0.52의 알려진 문제입니다.
- **확인 필요**: WSLg에서 Linux 창의 닫기 버튼으로 `--hold-window`가 풀리는지는 사용자 확인 항목입니다.

---

### English

On Linux the real 4th now passes its protection envelope into the original CRT and WinMain, creates its game window, and proceeds through DirectDraw initialization, stopping at `IDirectDraw7::CreateSurface` after 1,824 API calls on both widths with the game window on the host screen. This release also starts the DirectX core shared by Windows and Linux.

#### 1. Hardlock and the protection envelope (tasks 360–364, 367)
- **Shared Hardlock HLE**: Configuration assembly, `DeviceIoControl` completion rules, and device-path decisions moved into the shared core; the real 4th's Hardlock record on Windows is unchanged. WTS class 4 is corrected to `WTSSessionId`.
- **Guest devices and process environment**: The Linux `kernel32` facade opens `\\.\FEnteDev` and serves `DeviceIoControl`. `GuestProcess` keeps the guest's IDs, error mode, heaps, and page protections for image and `VirtualAlloc` regions; `advapi32` and `wtsapi32` facades joined.
- **The envelope's second layer**: Import-table reconstruction, the `ExitProcess` hook, `Read/WriteProcessMemory`, and thread timers carry the guest to the original entry point, with descriptor and transform counts matching Windows.

#### 2. From the original CRT to WinMain (tasks 368–371)
- **MSVC CRT start-up**: Heaps, start-up info, command line and environment, code page 949 (measured), and module paths; the stop stub is excluded from guest SEH.
- **Static initialization**: Unnamed events, a host clock with the time exports, and `GetTimeZoneInformation`; the game's own Hardlock login passes too.
- **Guest files and winmm**: Guest files come from the CHD with a copy-on-write overlay (`EZ2DJ.ini` and others), with `timeBeginPeriod`/`timeGetTime`.
- **API call log**: Every facade call records the inputs read from the guest, the outputs written back, the last error, and the return value in `logs/re2dj-<time>.api.log`; Hardlock buffers keep only their lengths.

#### 3. Windows and DirectDraw (tasks 372–374, 377, 380)
- **Guest calls**: HLE handlers can run a guest function such as a window procedure to completion — directly on x86, through a nested compatibility-mode transition on x64. APIs the guest calls meanwhile dispatch as nested calls, indented in the API log.
- **Window creation**: `RegisterClassA`, `CreateWindowExA`, `DefWindowProcA`, `UpdateWindow`, and `gdi32!GetStockObject`, with the creation messages and arguments measured on Windows 11.
- **DirectDraw entry**: Facade COM objects (vtables filled from `"<interface>::<method>"` exports); `DirectDrawEnumerateExA` (one monitor), `DirectDrawCreateEx`, `IDirectDraw7`, and `IDirect3D7`'s enumerations and caps, `SetCooperativeLevel`, and `SetDisplayMode` work.
- **Shared DirectX core (`re2dj_directx`)**: The 32-bit guest ABI structures, caps, device/format/display-mode enumerations, and cooperative-level and display-mode rules are shared by the Windows COM facade and the Linux gate facade. Windows checks the structures and constants against the SDK with `static_assert`, and the real 4th's DirectX record on Windows is unchanged.
- **Linux host window**: An SDL3/OpenGL window (640×480) opens when the game calls `SetCooperativeLevel`; it is black until drawing arrives. `--hold-window` keeps it open after the run stops.

#### 4. Product naming and logging (tasks 365–366, 375–376)
- **Version banner**: The window title, OSD, `--version`, `--help`, run log, and diagnostic tools all show the OS, architecture, and build type, as in `re2DJ v0.0.53 (Win/x86 Debug)`; both platforms build the window title with one function.
- **spdlog**: CLI run output and the Windows injected runtime's logs moved to spdlog.

#### 5. Structure (tasks 378–379)
- **Native helper IPC removed**: The Linux i386 helper and `--linux-helper`, the Windows native helper (optional build), the helper protocol, and their presets and scripts are gone; Linux runs in-process only, on both widths.
- **Platform boundary**: Linux code and targets no longer reference files under `src/platform/windows`; the shared probe fixture moved to `src/platform/native_probe_fixture`.

#### 6. Other
- **Fix**: The Windows `re2dj_windows_vfs_runtime_probe` crash is fixed (task 362), the known issue of v0.0.52.
- **To confirm**: Whether the Linux window's close button releases `--hold-window` under WSLg is left for the user to confirm.

---

## v0.0.52 (2026-09-24)

### 한국어

Linux에서 원본 PE32를 별도 helper 없이 같은 프로세스 안에서 실행합니다. x86·x86-64 두 host 모두 실제 4th CHD의 보호 stub을 Hardlock 오류 대화상자와 `ExitProcess(9)`까지 원본 코드로 진행합니다.

#### 1. 게스트 PE 호환 모듈 (작업 340~344, 348)
- **module registry와 PE32 facade**: `kernel32`, `user32` 같은 Win32 DLL을 DLL별 export 명세에서 만든 실제 PE32 facade image로 게스트에 보여 줍니다. 정적 IAT와 동적 `GetModuleHandleA`/`GetProcAddress`가 같은 export thunk 주소로 모입니다. 이전의 pseudo handle(`0x7F000001`)은 제거했습니다.
- **facade export**: `kernel32`의 `GetModuleHandleA`, `GetProcAddress`, `GetVersion`, `CreateFileA`, `ExitProcess`와 `user32`의 `GetActiveWindow`, `MessageBoxA`를 제공합니다.
- **종료 계약**: HLE 반환 구조 `ImportReturn`에 "게스트로 돌아가지 않고 프로세스가 끝난다"는 `exit_process`/`exit_code`를 추가했습니다. `ExitProcess`가 이를 쓰며, Linux in-process 실행은 이를 정상 종료로 보고합니다.

#### 2. Linux in-process 실행 (작업 345·349~352, 2026-09-22~23)
- **연속 실행과 게스트 SEH**: facade 위에서 원본을 계속 실행합니다. 첫 미처리 import, 미해석 lookup, fault, 종료에서 멈추고, 그때까지의 API 호출 기록을 출력합니다. 게스트 자신의 `INT3`는 게스트가 등록한 SEH handler로 전달하고, handler가 고친 CONTEXT로 재개합니다.
- **플랫폼 트리 비트 폭 분리**: `src/platform/linux/`를 두 폭 공용 루트와 `x86/`·`x64/` 구현으로 나눴습니다.

#### 3. Linux x86-64 compatibility-mode 실행 (작업 353~357)
- **같은 프로세스 실행**: x86-64 host가 CPU compatibility mode(CS `0x23`)로 32비트 게스트 코드를 직접 실행합니다. 게스트 FS(TEB)와 glibc TLS가 충돌하지 않도록, host로 돌아오는 모든 경로에서 host FS base를 복원합니다(`wrfsbase` 또는 `arch_prctl`).
- **x86과 같은 코드 경로**: PE session, import thunk, runner, facade, kernel32 진단, 게스트 SEH, instruction trace를 두 폭이 공유합니다. 실제 4th CHD의 진단 다섯 개가 x86과 같은 결과를 냅니다.
- **기본 실행 전환**: Linux의 `re2dj --run`은 이제 두 폭 모두 in-process로 실행합니다. 별도 i386 helper는 `--linux-helper <path>`로 고르는 진단 fallback입니다.

#### 4. 기타
- **로깅 표준화**: 런타임 로그를 spdlog 기반으로 통일했습니다(작업 345, 2026-09-21).
- **저장소 정책**: 런타임 산출물 ignore 정책(작업 346)과 플랫폼 비트 폭 디렉터리 규칙(작업 347)을 정했습니다.
- **수정**: x86 bootstrap이 게스트 FS용 TLS GDT 슬롯을 반환하지 않던 누수를 고쳤습니다. 한 프로세스에서 세 번째 실행부터 실패하던 문제입니다.
- **알려진 문제**: Windows `re2dj_windows_vfs_runtime_probe`는 이 릴리즈 이전부터 실패합니다(TODO의 기존 항목).

---

### English

Linux now runs the original PE32 in the same process without a separate helper. On both x86 and x86-64 hosts, the real 4th CHD's protection stub runs on original code through to its Hardlock error dialog and `ExitProcess(9)`.

#### 1. Guest PE compatibility modules (tasks 340–344, 348)
- **Module registry and PE32 facades**: Win32 DLLs such as `kernel32` and `user32` appear to the guest as real PE32 facade images built from per-DLL export declarations. Static IAT slots and dynamic `GetModuleHandleA`/`GetProcAddress` converge on the same export-thunk addresses; the former pseudo handle (`0x7F000001`) is gone.
- **Facade exports**: `kernel32` provides `GetModuleHandleA`, `GetProcAddress`, `GetVersion`, `CreateFileA`, and `ExitProcess`; `user32` provides `GetActiveWindow` and `MessageBoxA`.
- **Exit contract**: The HLE return structure `ImportReturn` gains `exit_process`/`exit_code`, meaning the call does not return because the process ends. `ExitProcess` uses it, and Linux in-process runs report it as a normal exit.

#### 2. Linux in-process execution (tasks 345 and 349–352, 2026-09-22–23)
- **Continuation and guest SEH**: The original keeps running on the facades until the first unhandled import, unresolved lookup, fault, or exit, printing the API call record up to that point. The guest's own `INT3` is delivered to its registered SEH handler and resumed with the CONTEXT the handler edited.
- **Platform tree split by host width**: `src/platform/linux/` is divided into a root shared by both widths and `x86/`/`x64/` implementations.

#### 3. Linux x86-64 compatibility-mode execution (tasks 353–357)
- **Same-process execution**: The x86-64 host runs 32-bit guest code directly in CPU compatibility mode (CS `0x23`). To keep the guest FS (TEB) from colliding with glibc TLS, every path back to the host restores the host FS base (`wrfsbase` or `arch_prctl`).
- **One code path with x86**: Both widths share the PE session, import thunks, runner, facades, kernel32 diagnostic, guest SEH, and instruction trace; all five real-4th-CHD diagnostics match x86.
- **Default run switched**: Linux `re2dj --run` now runs in-process on both widths; the separate i386 helper is a diagnostic fallback selected with `--linux-helper <path>`.

#### 4. Other
- **Logging**: Standardized runtime logging on spdlog (task 345, 2026-09-21).
- **Repository policy**: Defined the runtime-artifact ignore policy (task 346) and the platform bit-width directory rules (task 347).
- **Fix**: Fixed an x86 bootstrap leak that never returned the guest-FS TLS GDT slot, which made a third run in one process fail.
- **Known issue**: The Windows `re2dj_windows_vfs_runtime_probe` fails independently of this release (an existing TODO item).

---

## v0.0.48 (2026-09-18)

### 한국어

- **문서 보완**: 작업 291~297에서 확인한 원본 분석 결과(보호 빌드 복호화 시점, helper RVA 대조, 3rd 설정 레지스트리·데모·autoplay 플래그, 프레임 pacing)를 `EXE_DESIGN`에 누적하고, Win32 커서·자식 창 입력 배경 문서와 후속 TODO를 추가했습니다. 코드 변경은 없습니다.

---

### English

- **Documentation**: Accumulated the original-analysis findings of tasks 291-297 (protected-build decryption timing, helper RVA cross-check, 3rd settings registry, demo and autoplay flags, frame pacing) into `EXE_DESIGN`, and added a background topic on Win32 cursor and child-window input plus follow-up TODO entries. No code change.

---

## v0.0.47 (2026-09-17)

### 한국어

- **Dear ImGui 기반 On-Screen Display(OSD) 추가**: 실행 중 백틱(`` ` ``) 키로 토글할 수 있는 가벼운 OSD를 도입했습니다. 숨김 상태에서는 ImGui 프레임을 구성하지 않아 렌더링 비용이 발생하지 않습니다. 화면 상단에 버전, 빌드 일시, 타깃 프로파일, 실행 파일 이름을 표시합니다.
- **EZ2DJ 3rd Trax 자율 연주(Autoplay) 토글 지원**: 복호화 덤프 분석으로 확인된 내부 autoplay 플래그 주소(`0x00629508`)를 타깃 프로파일의 `game_controls`에 등록했습니다. 런처는 실행 파일의 빌드 timestamp(`0x3bca98a3`)가 일치할 때만 주소를 주입 런타임에 전달해 안전하게 무장하며, 곡 시작 전에 OSD에서 체크하면 해당 곡이 자동으로 연주됩니다. 데모 오버레이나 음소거 등 데모 플레이 부작용이 없습니다.
- **마우스 커서 표시 복구**: 3rd가 커서를 숨기더라도 클라이언트 영역 위에서 마우스 커서가 유지되어 OSD를 조작할 수 있도록 했습니다.

---

### English

- **Dear ImGui On-Screen Display (OSD)**: Introduced a lightweight OSD toggled with backtick (`` ` ``). No ImGui frame is built while hidden, incurring zero rendering overhead. Displays version, build timestamp, target profile, and executable name across the top of the window.
- **Autoplay Toggle for EZ2DJ 3rd Trax**: Registered the internal autoplay flag address (`0x00629508`) in target profile `game_controls`. Armed only when the executable build timestamp (`0x3bca98a3`) matches. Ticking Autoplay before song start plays the song autonomously without demo-play side effects.
- **Mouse Cursor Restoration**: Restored the arrow cursor over the client area to ensure easy interaction with the OSD despite 3rd hiding the cursor.

---

## v0.0.31 (2026-09-07)

### 한국어

런타임 핫 경로 성능 개선: CHD/FAT32 판독 캐시, OpenGL draw 경계 고정 비용 제거, draw 경로 진단 게이트

게스트가 관찰하는 바이트, 픽셀, 상태는 바뀌지 않습니다. 성능 특성만 바뀝니다.

#### 1. CHD/FAT32 판독 캐시 (작업 219)
- **CHD hunk 캐시 추가**: 압축 해제된 hunk의 LRU 캐시 `re2dj::storage::ChdHunkCache`를 저장소 계층의 독립 구성요소로 추가했습니다. `libchdr`의 `chd_read`는 호출마다 다시 압축을 풀기 때문에, 512바이트 sector 하나를 읽을 때마다 4,096바이트 hunk 전체를 LZMA 해제하던 비용을 이 계층이 흡수합니다.
- **FAT32 조회 캐시 추가**: `Fat32Volume`에 디렉터리 항목, 해석된 경로, 파일 클러스터 체인 캐시를 넣었습니다. `ReadFileRange`가 호출마다 경로를 다시 해석하고 FAT 체인을 첫 클러스터부터 다시 걷던 제곱 동작을 제거했고, 중간 클러스터 버퍼 없이 목적지 버퍼로 직접 판독합니다.
- **판독 경로 직렬화**: 게스트가 여러 스레드에서 파일 API를 호출하므로 공개 판독 API를 `std::mutex`로 직렬화했습니다. 이전에는 잠금이 없었고 `libchdr`의 내부 버퍼가 공유 상태였습니다.
- 실제 4th CHD에 대한 `re2dj_chd_probe` 출력이 변경 전후 바이트 단위로 동일함을 확인했습니다.

#### 2. OpenGL draw 경계 고정 비용 제거 (작업 220)
- draw 1회마다 반복되던 `SDL_GL_MakeCurrent` 1회, `glGetUniformLocation` 5회, 정점 속성 배열 활성/비활성 6회, `glTexParameteri` 4회, `std::vector` 힙 할당 1회를 제거했습니다.
- uniform location은 프로그램 링크 직후 한 번만 조회하고, 정점 변환 버퍼는 재사용하며, 텍스처 샘플러 상태는 값이 실제로 바뀔 때만 설정합니다.
- draw별 `glGetError`는 초기 256 draw와 진단 실행으로 한정합니다. `Present`의 프레임 단위 검사는 그대로 유지하므로 지속적인 GL 실패는 계속 검출됩니다.

#### 3. draw 경로 진단 게이트 (작업 221)
- **`--graphics-draw-diagnostics` 옵션 추가**: draw 경로 안에서 실행되던 `ReportDrawDiagnostic`, `ReportLateDrawDiagnostic`, `ReportTransformDiagnostic`을 새 스위치 뒤로 옮겼습니다. 기본값은 꺼짐이며 제품 실행 경로는 이 비용을 지불하지 않습니다.
- 실제 4th CHD 실행에서 `.ddraw.log`가 21,117줄에서 631줄로 줄었고, 텍스처 전 픽셀 스캔과 긴 레코드 포맷팅을 유발하던 draw 단위 항목 20,480건이 사라졌습니다. 초기화 진단은 그대로 기록됩니다.
- draw 단위 증거가 필요한 조사에서는 이 옵션을 명시적으로 켭니다. 관련 가이드와 분석 문서를 함께 갱신했습니다.

#### 4. 문서
- [런타임 핫 경로 성능 설계](docs/design/20260907-219-runtime-performance-hot-paths.md), 작업 지시 3건, 작업 로그 3건을 추가했습니다.
- `libchdr`에 hunk 캐시가 없다는 일반 기술 배경을 [docs/kb/mame-chd-hunk-decompression.md](docs/kb/mame-chd-hunk-decompression.md)에 정리했습니다.
- 4th CHD의 codec 목록과 hunk 수를 분석 문서에 반영하고, CHD 파일 이름이 인식 조건이 아니라는 점을 명시했습니다.

---

### English

Runtime hot-path performance: CHD/FAT32 read caches, per-draw fixed cost removal in the OpenGL boundary, and a gate for the draw-path diagnostics.

The bytes, pixels, and state the guest observes are unchanged; only performance characteristics change.

#### 1. CHD/FAT32 Read Caches (Task 219)
- **CHD hunk cache**: Added `re2dj::storage::ChdHunkCache`, an LRU of decompressed hunks, as its own component in the storage layer. `libchdr`'s `chd_read` decompresses on every call, so reading one 512-byte sector fully decompressed a 4,096-byte LZMA hunk; that cost is now absorbed here.
- **FAT32 lookup caches**: Added directory-entry, resolved-path, and cluster-chain caches to `Fat32Volume`. This removes the quadratic behavior where `ReadFileRange` re-resolved the path and re-walked the FAT chain from the first cluster on every call, and reads now go straight into the caller's destination without an intermediate cluster buffer.
- **Serialized read path**: The public read API is now guarded by a `std::mutex`, since the guest calls the file APIs from several threads; previously there was no lock and `libchdr`'s internal buffers were shared state.
- Verified that `re2dj_chd_probe` output on the real 4th CHD is byte-identical before and after.

#### 2. OpenGL Draw Boundary Fixed Cost (Task 220)
- Removed the per-draw `SDL_GL_MakeCurrent`, five `glGetUniformLocation` lookups, six vertex-attribute-array toggles, four `glTexParameteri` calls, and one vector allocation.
- Uniform locations are resolved once after link, the vertex conversion buffer is reused, and texture sampler state is applied only when a value actually changes.
- The per-draw `glGetError` now runs for the first 256 draws and during diagnostic runs; `Present` keeps its unconditional per-frame check, so a persistently broken GL state is still detected.

#### 3. Draw-Path Diagnostic Gate (Task 221)
- **Added `--graphics-draw-diagnostics`**: `ReportDrawDiagnostic`, `ReportLateDrawDiagnostic`, and `ReportTransformDiagnostic` now sit behind a switch that defaults to off, so the product execution path does not pay for them.
- On the real 4th CHD the `.ddraw.log` dropped from 21,117 lines to 631, removing the 20,480 per-draw entries that drove whole-surface texel scans and long record formatting. Initialization diagnostics still record.
- Investigations that need draw-level evidence turn the option on explicitly; the related guides and analysis documents were updated accordingly.

#### 4. Documentation
- Added the [runtime hot-path performance design](docs/design/20260907-219-runtime-performance-hot-paths.md), three work orders, and three work logs.
- Recorded the absence of a hunk cache in `libchdr` as general background in [docs/kb/mame-chd-hunk-decompression.md](docs/kb/mame-chd-hunk-decompression.md).
- Recorded the 4th CHD codec set and hunk count in the analysis document, and noted that the CHD file name is not part of recognition.

---

## v0.0.30 (2026-09-06)

### 한국어

EZ2DJ 3rd Trax (MAME CHD) 완전 실행(그래픽 60 FPS, 사운드, 키보드 조작) 지원 및 DirectDraw 7 HLE 계층 구현

#### 1. EZ2DJ 3rd Trax 완전 실행 지원
- **MAME CHD 기반 VFS 마운트**: `roms/ez2dj3rd/ez2dj3rd.chd` 이미지를 직접 인식하여 FAT32 파일시스템 상의 `EZ2DJ/EZ2DJ.EXE` 및 리소스(BG, Sound, System)를 동적으로 읽어 실행하도록 지원.
- **오디오 출력**: DirectSound HLE 스트리밍 링 버퍼를 통해 BGM 및 키음의 정상 출력을 확인.

#### 2. DirectDraw 7 HLE 및 윈도우 모드 프레젠테이션
- **DirectDraw 7 계층 구현**: `DirectDrawCreateEx` thunk 및 `IDirectDraw7` 인터페이스 구현.
- **클리퍼 및 프라이머리 서피스 분리**: `CreateClipper` 및 `SetHWnd` 지원, 백버퍼 없는 단독 `PrimarySurface` 생성 지원.
- **윈도우 모드 60 FPS 프레임 표시**: 윈도우 모드 데스크톱 화면 좌표계를 수용하고, `PrimarySurface->Blt` 호출 시 OpenGL FBO 버퍼를 호스트 SDL 창으로 스왑(`Present`)하도록 연동하여 안정적인 60 FPS 화면 갱신을 달성.

#### 3. I/O 포트 에뮬레이션 및 키보드 입력 지원
- **3rd 전용 Legacy I/O 헬퍼 RVA 확정**: 메모리 역어셈블 분석을 통해 `in al, dx`(`0x000a9887`) 및 `out dx, al`(`0x000a98bb`) 헬퍼 주소를 확정하고 포트(`0x101`~`0x106`)를 에뮬레이션 버스에 연결.
- **키보드 파싱 확장**: `config/ez2dj-io.example.ini`의 1P 턴테이블 키(`TAB`)를 비롯하여 `ESC`, `SHIFT`, `CTRL`, `ALT`, `BACKSPACE`, `CAPS`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN` 등의 파싱 지원 추가.
- **설정 파일 경로 정규화**: `--io-config` 인자를 절대 경로로 자동 변환하여 게스트 작업 디렉터리 경로 불일치 문제 해결. 코인 투입(F5), 스타트(1/2), 건반, 턴테이블, 페달 조작 지원.

#### 4. 릴리즈 빌드 스크립트 추가
- 최적화된 바이너리를 빌드하고 검증하기 위한 `scripts/build_release.bat` 및 `scripts/build_release.ps1` 추가.

---

### English

Full execution support for EZ2DJ 3rd Trax (60 FPS graphics, audio, and keyboard controls) and DirectDraw 7 HLE implementation.

#### 1. Full EZ2DJ 3rd Trax Execution Support
- **MAME CHD-Backed VFS Mount**: Directly mounts `roms/ez2dj3rd/ez2dj3rd.chd`, dynamically reading `EZ2DJ/EZ2DJ.EXE` and game assets (BG, Sound, System) from the FAT32 volume.
- **Audio Playback**: Confirmed pristine BGM and key sound playback through the DirectSound HLE streaming ring buffer.

#### 2. DirectDraw 7 HLE & Windowed Presentation
- **DirectDraw 7 Layer Implementation**: Added `DirectDrawCreateEx` export thunk and `IDirectDraw7` interface wrapper.
- **Clipper & Standalone Primary Surface**: Added support for `CreateClipper`, `SetHWnd`, and standalone `PrimarySurface` creation without backbuffers.
- **Windowed 60 FPS Frame Presentation**: Accepts screen-space desktop coordinates and triggers host SDL window buffer swaps (`Present`) inside `PrimarySurface->Blt`, achieving smooth and stable 60 FPS rendering.

#### 3. I/O Port Emulation & Keyboard Input Support
- **Confirmed 3rd Legacy I/O Helper RVAs**: Identified `in al, dx` (`0x000a9887`) and `out dx, al` (`0x000a98bb`) helper instruction RVAs through memory disassembly, mapping ports `0x101` through `0x106` to the I/O bus.
- **Extended Keyboard Key Parsing**: Added support in `ParseKey` for `TAB` (`p1_positive=TAB`), `ESC`, `SHIFT`, `CTRL`, `ALT`, `BACKSPACE`, `CAPS`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGEUP`, and `PAGEDOWN`.
- **Configuration Path Canonicalization**: Canonicalized `--io-config` paths to absolute paths, resolving guest working directory path mismatches and enabling full control for Coin (F5), Start (1/2), Keys, Turntables, and Pedal.

#### 4. Release Build Scripts
- Added `scripts/build_release.bat` and `scripts/build_release.ps1` for building and verifying optimized Release binaries.
