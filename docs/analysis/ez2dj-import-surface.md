# EZ2DJ import 표면 / EZ2DJ Import Surface

주제: 원본 실행 파일이 실제로 호출하는 Win32 API 집합. HLE 구현 범위를 정하는 근거 문서다.

*Topic: the set of Win32 APIs the original executable actually calls. This is the document that fixes the HLE implementation scope.*

측정 대상: `ez2dj1.exe` (1st Trax Special Edition 덤프, 1999-12-24 빌드, 보호되지 않음). 측정 방법: PE import 디렉터리 직접 해석. 근거는 [HDD 레이아웃 분석](ez2dj-hdd-layout.md)에 있다.

*Measured on `ez2dj1.exe`, the unprotected 1999-12-24 build in the 1st SE dump, by parsing the PE import directory directly.*

---

## 1. 확인됨: 전체 규모 / Confirmed: total size

**7개 DLL, 144개 함수.**

| DLL | 함수 수 | 성격 |
| --- | --- | --- |
| `KERNEL32.dll` | 97 | 대부분 MSVC CRT 시작 코드 |
| `USER32.dll` | 21 | 창, 메시지 루프, 키보드, 디스플레이 모드 |
| `GDI32.dll` | 14 | DIB 섹션과 블릿 |
| `WINMM.dll` | 8 | 믹서 볼륨과 밀리초 타이머 |
| `DDRAW.dll` | 2 | DirectDraw 생성과 열거 |
| `DSOUND.dll` | 1 | ordinal `#1` = `DirectSoundCreate` |
| `ADVAPI32.dll` | 1 | `RegFlushKey` |

144개는 예상보다 **훨씬 작다.** 이 프로젝트의 HLE 범위가 감당 가능한 크기라는 뜻이다. 게다가 97개의 KERNEL32 항목 중 상당수가 CRT 시작 코드가 부르는 것이고 게임 로직이 직접 쓰는 것이 아니다.

*144 is **far smaller** than one might assume, which means the HLE scope for this project is tractable. Many of the 97 KERNEL32 entries are called by CRT startup rather than by game logic.*

---

## 2. 확인됨: 그래픽 / Confirmed: graphics

```text
DDRAW.dll   DirectDrawCreate, DirectDrawEnumerateA
GDI32.dll   CreateDIBSection, BitBlt, StretchBlt, CreateCompatibleDC, SelectObject,
            DeleteDC, DeleteObject, GetObjectA, GetStockObject, CreateSolidBrush,
            SetBkMode, SetBkColor, SetTextColor, ExtTextOutA
USER32.dll  EnumDisplaySettingsA, ChangeDisplaySettingsExA, DrawTextA, FillRect,
            SetRect, ReleaseDC, LoadImageA
```

**Direct3D가 없다.** `DirectDrawCreate`만 있고 `DirectDrawCreateEx`도 없으므로 DirectDraw 1~6 인터페이스다. 3D 파이프라인, 텍스처 스테이지, 셰이더를 HLE로 제공할 필요가 없다.

렌더링은 2D 표면 블릿과 GDI DIB 섹션 조합이다. `ChangeDisplaySettingsExA`와 `EnumDisplaySettingsA`가 있으므로 전체 화면 모드를 직접 설정한다.

*No Direct3D. Only `DirectDrawCreate` appears — not even `DirectDrawCreateEx` — so this is the DirectDraw 1 through 6 interface family. No 3D pipeline, texture stages, or shaders need HLE. Rendering is 2D surface blitting combined with GDI DIB sections, and the display-settings calls mean the program sets its own full-screen mode.*

이것이 HLE 범위에 주는 영향은 크다. `ARCHITECTURE.md`가 우선순위 2로 잡았던 `d3d`는 **1st에 관한 한 필요 없다.**

*This materially shrinks the scope: the `d3d` entry `ARCHITECTURE.md` listed at priority 2 is **not needed at all for 1st**.*

---

## 3. 확인됨: 오디오 / Confirmed: audio

```text
DSOUND.dll  #1                       (DirectSoundCreate)
WINMM.dll   mixerOpen, mixerClose, mixerGetNumDevs, mixerGetLineInfoA,
            mixerGetLineControlsA, mixerGetControlDetailsA, mixerSetControlDetails,
            timeGetTime
```

DirectSound는 **ordinal import**다. 이름이 아니라 번호로 들어오므로 HLE 모듈 테이블이 ordinal 대조를 지원해야 한다. `DSOUND.dll`의 ordinal `#1`은 `DirectSoundCreate`다.

`winmm`의 mixer 계열 일곱 개는 전부 볼륨 제어다. 아케이드 캐비닛의 마스터 볼륨을 게임이 직접 조정한다. `timeGetTime`이 유일한 타이밍 소스다.

*DirectSound arrives as an **ordinal import**, so the HLE module table must match on ordinals as well as names; `DSOUND.dll` ordinal `#1` is `DirectSoundCreate`. The seven `winmm` mixer calls are all volume control — the game drives the cabinet master volume itself — and `timeGetTime` is the only timing source.*

---

## 4. 확인됨: 입력 / Confirmed: input

```text
USER32.dll  GetAsyncKeyState
```

**DirectInput이 없다.** 입력 전체가 `GetAsyncKeyState` 하나다. 아케이드 발판과 버튼이 키보드 코드로 들어온다는 뜻이므로, 입력 HLE는 키 상태 배열 하나면 된다.

*No DirectInput. All input is a single `GetAsyncKeyState`, so cabinet buttons arrive as keyboard codes and the input HLE needs only a key-state array.*

3rd는 다르다. 3rd의 보호 스텁 import에 `DINPUT.dll: DirectInputCreateA`가 있다. 버전별 차이이므로 HLE 프로파일로 분리해야 한다.

*3rd differs: its protection stub imports `DINPUT.dll: DirectInputCreateA`. That is a per-version difference and belongs in an HLE profile.*

---

## 5. 확인됨: 파일 시스템과 설정 / Confirmed: file system and configuration

```text
CreateFileA, ReadFile, WriteFile, CloseHandle, SetFilePointer, SetEndOfFile,
GetFileSize, GetFileType, FlushFileBuffers,
FindFirstFileA, FindNextFileA, FindClose,
GetCurrentDirectoryA, SetCurrentDirectoryA, GetWindowsDirectoryA, GetModuleFileNameA,
GetPrivateProfileStringA, GetPrivateProfileIntA, GetPrivateProfileSectionNamesA,
WritePrivateProfileStringA
```

파일 I/O는 Win32 핸들 API를 직접 쓴다. 디렉터리 열거도 있으므로 가상 파일 시스템이 `FindFirstFileA` 계열을 지원해야 한다.

설정은 INI다. `GetPrivateProfile*` 계열이 덤프의 `ez2dj.ini`, `System.ini`를 읽는다. `WritePrivateProfileStringA`가 있으므로 **게스트가 INI에 쓴다.** overlay 쓰기 정책이 실제로 동작해야 한다는 뜻이다.

`SetCurrentDirectoryA`가 있으므로 게스트 현재 디렉터리가 실행 중에 바뀐다. 경로 해석이 현재 디렉터리를 추적해야 한다.

*File I/O uses the Win32 handle APIs directly, and directory enumeration means the virtual file system must support the `FindFirstFileA` family. Configuration is INI-based and the guest **writes** to it, so the overlay write policy has to actually work. `SetCurrentDirectoryA` means the guest current directory changes during a run and path resolution must track it.*

모든 API가 ANSI(`...A`) 계열이다. 문자열이 CP949일 가능성이 높다는 [Win32 HLE 경계](../kb/win32-hle-boundary.md)의 서술과 일치한다.

*Every API is the ANSI (`...A`) variant, consistent with the CP949 expectation recorded in [Win32 HLE Boundary](../kb/win32-hle-boundary.md).*

---

## 6. 확인됨: 스레드와 프로세스 / Confirmed: threads and processes

```text
CreateThread, TerminateThread, SetThreadPriority, GetCurrentThread, GetCurrentThreadId,
CreateEventA, SetEvent, WaitForSingleObject, Sleep,
InitializeCriticalSection, EnterCriticalSection, LeaveCriticalSection, DeleteCriticalSection,
TlsAlloc, TlsFree, TlsGetValue, TlsSetValue,
InterlockedIncrement, InterlockedDecrement,
CreateProcessA, ExitProcess, TerminateProcess
```

**게스트는 멀티스레드다.** 실행 backend가 스레드 하나만 가정하면 안 된다. 다만 스레드가 몇 개이고 무엇을 하는지는 미확정이며, 실행해 봐야 안다.

`CreateProcessA`가 있으므로 다른 실행 파일을 띄운다. 덤프에 `PlzPowerOff.exe`와 `Test.exe`가 함께 있으므로 그 둘일 가능성이 높으나 확인되지 않았다.

*The guest is **multithreaded**, so the execution backend must not assume a single thread. How many threads there are and what they do is unresolved until a run. `CreateProcessA` means it launches other executables, likely `PlzPowerOff.exe` and `Test.exe` given they sit in the same directory, but that is not confirmed.*

`USER32.dll: ExitWindowsEx`도 있다. 게임이 Windows 자체를 종료시킨다. HLE에서는 당연히 호스트를 종료시키지 않고 게스트 실행만 끝낸다.

*`ExitWindowsEx` is imported too: the game shuts Windows down. The HLE obviously must end the guest run instead of shutting the host down.*

---

## 7. 확인됨: 레지스트리 / Confirmed: registry

```text
ADVAPI32.dll  RegFlushKey
```

레지스트리 API가 `RegFlushKey` **하나뿐이다.** 열지도 읽지도 쓰지도 않고 flush만 한다. Windows 9x에서 전원이 끊기기 전에 레지스트리를 디스크로 밀어내는 용도로 보인다. HLE 구현은 성공을 반환하는 stub이면 충분하다.

*The only registry API is `RegFlushKey` — nothing opens, reads, or writes. It appears to push the registry to disk before power is cut on Windows 9x, so a stub returning success suffices.*

---

## 8. 결론: HLE 우선순위 / Conclusion: HLE priority

이 목록이 확정한 구현 순서다. 추측이 아니라 import 테이블에서 나왔다.

*The implementation order this list fixes. It comes from the import table, not from guesswork.*

| 순위 | 대상 | 규모 | 근거 |
| --- | --- | --- | --- |
| 1 | `kernel32` 파일·메모리·INI | 약 40개 | 자산을 못 읽으면 아무것도 진행되지 않는다 |
| 1 | `user32` 창과 메시지 루프 | 약 12개 | 창이 없으면 프레임이 없다 |
| 2 | `ddraw` DirectDraw 1~6 | COM 인터페이스 | 렌더링 전부 |
| 2 | `gdi32` DIB 섹션과 블릿 | 14개 | DirectDraw와 함께 쓰인다 |
| 3 | `dsound` (ordinal `#1`) | COM 인터페이스 | 소리 |
| 3 | `winmm` mixer + `timeGetTime` | 8개 | 볼륨과 타이밍 |
| 4 | `user32` `GetAsyncKeyState` | 1개 | 입력 전부 |
| 4 | `advapi32` `RegFlushKey` | 1개 | stub |
| 5 | `kernel32` 스레드·이벤트 | 약 20개 | 실행 backend의 스레드 모델이 먼저 필요하다 |

가장 큰 작업은 함수 개수가 아니라 **DirectDraw와 DirectSound의 COM 인터페이스**다. 두 API 모두 게스트 메모리 안에 vtable을 만들어 각 슬롯에 gate 주소를 채워야 한다.

*The largest piece is not the function count but the **COM interfaces of DirectDraw and DirectSound**, which both need a vtable built inside guest memory with a gate address in each slot.*

---

## 9. 확인됨: 보호 빌드의 정적 import 표면 / Confirmed: the protected build's static import surface

**확인됨 — 2026-08-23.** 보호된 `ez2dj.exe`의 import directory는 `.gidata`(RVA `0x01ad8000`)로 옮겨져 있으며, 내용은 8절의 원본 표면 전체에 보호 특화 API를 더한 집합이다. `.gidata`의 IMAGE_IMPORT_DESCRIPTOR와 IAT를 직접 해석해 슬롯 VA까지 확정했다.

추가된 보호 특화 import:

```text
KERNEL32.dll  DeviceIoControl, _lopen, _lread, _lclose, _lcreat, _llseek, _lwrite,
              OpenFile, DeleteFileA, MoveFileA, lstrcmpiA, lstrcpyA, lstrlenA,
              DebugBreak, OutputDebugStringA, FatalAppExitA, UnhandledExceptionFilter,
              RaiseException, RtlUnwind, IsBadReadPtr, IsBadWritePtr, HeapValidate,
              CreateMutexA, ReleaseMutex
```

`.gdata`에는 `\\.\TDSD.VXD`, `\\.\LPTDI0`, `MSVBVM50.DLL`(2회), EUC-KR 메시지 바이트, 해시성 blob이 있다. 런타임 관찰([HDD 레이아웃 분석](ez2dj-hdd-layout.md))에서 `CreateFileA("\\.\LPTDI1")` 병렬포트 열기와 `GetProcAddress(wsock32, "WSAGetLastError")`가 확인됐다. 즉 보호 계층의 동적 해석은 최소화되어 있고(관찰된 것은 WSAGetLastError 하나), 하드웨어·환경 검사는 정적 import와 문자열로 구성된다.

*Confirmed — 2026-08-23. The protected `ez2dj.exe` moves its import directory into `.gidata` (RVA 0x01ad8000) and its content is the full original surface of section 8 plus protection-flavored APIs: DeviceIoControl, the `_l*`/OpenFile legacy file family, DeleteFileA, MoveFileA, lstrcmpiA/lstrcpyA/lstrlenA, DebugBreak, OutputDebugStringA, FatalAppExitA, UnhandledExceptionFilter, RaiseException, RtlUnwind, IsBadReadPtr/IsBadWritePtr, HeapValidate, and mutex APIs. .gdata holds `\\.\TDSD.VXD`, `\\.\LPTDI0`, `MSVBVM50.DLL` (twice), EUC-KR message bytes, and hash-like blobs. Runtime observation (see the HDD layout analysis) confirms a `CreateFileA("\\.\LPTDI1")` parallel-port open and GetProcAddress for only WSAGetLastError, so dynamic resolution is minimal and hardware/environment checks are built from static imports plus strings.*

---

## 10. 미확정 / Unresolved

* 보호된 `ez2dj.exe`와 `EZ2DJ.EXE`가 런타임에 추가로 해석하는 API. 스텁이 `GetProcAddress`로 가져오므로 정적으로는 보이지 않는다. → 1st SE는 관찰상 `WSAGetLastError` 하나뿐. 다른 버전은 미확정.
* ~~`CreateFileA`가 실제로 여는 경로~~ → **확정됨**: 보호 stub이 `\\.\LPTDI1`을 연다. 게임 본체의 파일 경로는 여전히 미확정.
* 스레드 개수와 역할.
* DirectDraw 표면 구성: 표면 개수, 픽셀 포맷, 해상도.

*Unresolved: run-time `GetProcAddress` additions by the protected builds — 1st SE observes only WSAGetLastError; other versions are unresolved. The `CreateFileA` path question is now resolved for the protection stub (`\\.\LPTDI1`) but not for the game body. Still open: thread count and roles, and the DirectDraw surface configuration.*
