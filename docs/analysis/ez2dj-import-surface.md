# EZ2DJ import 표면 / EZ2DJ Import Surface

주제: 원본 실행 파일이 실제로 호출하는 Win32 API 집합. HLE 구현 범위를 정하는 근거 문서다.

*Topic: the set of Win32 APIs the original executable actually calls. This is the document that fixes the HLE implementation scope.*

측정 대상: 각 제품의 **정식 실행 파일**이다. 1st Tracks는 `Ez2DJ.exe`, 1st SE는
`ez2dj.exe`, 3rd는 `EZ2DJ.EXE`, 4th와 5th는 `EZ2DJ.exe`, EZ2Dancer 2nd MOVE는
`EZ2Dancer.exe`이며 모두 보호된 빌드다. 6th는 예외로, `EZ2DJ.EXE` bootstrap과
`EZ2DJ6th.EXE` 게임 본체 둘 다 보호되지 않는다. 측정 방법은 `re2dj_pe_analyzer`로
섹션과 데이터 디렉터리를 읽고, `re2dj_pe_loader`와 별도 IMAGE_IMPORT_DESCRIPTOR 해석
두 경로로 import table을 교차 확인하는 것이다. 두 경로의 1st SE 결과는 161개 항목이
이름까지 일치했다.

*Measured on each product's **canonical executable** — 1st Tracks `Ez2DJ.exe`, 1st SE
`ez2dj.exe`, 3rd `EZ2DJ.EXE`, 4th and 5th `EZ2DJ.exe`, and EZ2Dancer 2nd MOVE
`EZ2Dancer.exe`, all protected builds. 6th is the exception: neither its `EZ2DJ.EXE`
bootstrap nor its `EZ2DJ6th.EXE` game body is protected. Sections and
data directories are read with `re2dj_pe_analyzer`, and the import tables are cross-checked
through two independent paths, `re2dj_pe_loader` and a separate IMAGE_IMPORT_DESCRIPTOR
parse. For 1st SE both paths agreed on all 161 entries including names.*

---

## 1. 확인됨: 전체 규모 / Confirmed: total size

보호된 빌드는 import table을 **두 개** 가진다. 원본 `.idata` table과, PE 데이터 디렉터리가
가리켜 Windows loader가 실제로 bind하는 packed table이다. HLE가 처음에 마주하는 것은
후자다.

*A protected build carries **two** import tables: the original `.idata` table, and the packed
table named by the PE data directory, which is what the Windows loader actually binds. The
HLE meets the latter first.*

| 제품 / product | 원본 `.idata` | loader가 bind하는 packed table | packer 섹션 |
| --- | --- | --- | --- |
| 1st Tracks `Ez2DJ.exe` | 7 DLL / 141 | **22 항목** (`.protect`) | `.protect` |
| 1st SE `ez2dj.exe` | 7 DLL / 144 | **7 DLL / 161** (`.gidata`) | `.gtide` |
| 3rd `EZ2DJ.EXE` | 10 DLL / 159 | **36 항목** (`.protect`) | `.protect` |
| 4th `EZ2DJ.exe` | 10 DLL / 161 | **36 항목** (`.protect`) | `.protect` |
| 5th `EZ2DJ.exe` | 10 DLL / 161 | **36 항목** (`.protect`) | `.protect` |
| 6th `EZ2DJ.EXE` (bootstrap) | 2 DLL / 68 | 동일 / the same table | 없음 / none |
| 6th `EZ2DJ6th.EXE` | 7 DLL / 137 | 동일 / the same table | 없음 / none |
| 6th 동봉 1st `Ez2DJ.exe` | 6 DLL / 138 | 동일 / the same table | 없음 / none |
| `ez2d2m` `EZ2Dancer.exe` | `.idata` 섹션 없음 | **32 항목** (`.protect`) | `.protect` |

**확인됨 — 2026-09-15.** packed table의 "항목" 수는 이름 기준 고유 개수다. 직접
IMAGE_IMPORT_DESCRIPTOR 해석은 thunk 슬롯을 세므로 더 큰 값이 나온다. 1st Tracks는 슬롯
30개에 고유 22개(`GetProcAddress`, `GetModuleHandleA`, `RtlUnwind` 중복), 4th와 5th는 슬롯
38개에 고유 36개(`GetProcAddress`, `GetModuleHandleA` 중복)다. 두 해석 경로는 이 중복
때문에 다른 수를 보고하며 서로 모순되지 않는다.

*Confirmed — 2026-09-15. The packed-table "entry" counts are distinct names. A direct
IMAGE_IMPORT_DESCRIPTOR parse counts thunk slots and therefore reports more: 1st Tracks has
30 slots for 22 distinct names (`GetProcAddress`, `GetModuleHandleA` and `RtlUnwind`
duplicated), and 4th and 5th each have 38 slots for 36 distinct names (`GetProcAddress` and
`GetModuleHandleA` duplicated). The two parse paths disagree only because of these
duplicates.*

**확인됨 — 2026-09-15.** 6th의 세 실행 파일은 보호 섹션이 없으므로 import table이 하나뿐이고,
원본 표면이 곧 loader가 bind하는 표면이다. 특히 `EZ2DJ6th.EXE`와 6th 동봉 1st Tracks
빌드는 **unpack 없이 게임 표면을 그대로 읽을 수 있는** 유일한 입력이다.

*Confirmed — 2026-09-15. The 6th's three executables carry no protection section, so each has a
single import table and its original surface is the loader-bound surface. `EZ2DJ6th.EXE` and
the 6th's bundled 1st Tracks build are the only inputs whose **game surface can be read
without unpacking**.*

두 packer 계열이 다르게 동작한다. 1st SE의 `.gtide` packer는 원본 import table 전체를
`.gidata`로 **옮겨 보존**하므로 packed table이 곧 게임의 전체 표면이다. 3rd·4th·`ez2d2m`의
`.protect` packer는 DLL당 대표 stub 하나만 남기므로 packed table만으로는 게임 표면을 알 수
없고, 원본 `.idata`를 따로 읽거나 런타임 `GetProcAddress` 해석을 추적해야 한다.

*The two packer families behave differently. The 1st SE `.gtide` packer **relocates and
preserves** the whole original import table into `.gidata`, so its packed table is the game's
complete surface. The `.protect` packer used by 3rd, 4th and `ez2d2m` keeps only one
representative stub per DLL, so the packed table alone does not reveal the game surface —
the original `.idata` must be read separately, or runtime `GetProcAddress` resolution traced.*

### 1st SE 정식 빌드 상세 / 1st SE canonical build detail

| DLL | 원본 `.idata` | packed `.gidata` | 성격 |
| --- | --- | --- | --- |
| `KERNEL32.dll` | 97 | 113 | 대부분 MSVC CRT 시작 코드 |
| `USER32.dll` | 21 | 22 | 창, 메시지 루프, 키보드, 디스플레이 모드 |
| `GDI32.dll` | 14 | 14 | DIB 섹션과 블릿 |
| `WINMM.dll` | 8 | 8 | 믹서 볼륨과 밀리초 타이머 |
| `DDRAW.dll` | 2 | 2 | DirectDraw 생성과 열거 |
| `DSOUND.dll` | 1 | 1 | ordinal `#1` = `DirectSoundCreate` |
| `ADVAPI32.dll` | 1 | 1 | `RegFlushKey` |
| 합계 | **144** | **161** | |

161개는 예상보다 **훨씬 작다.** 이 프로젝트의 HLE 범위가 감당 가능한 크기라는 뜻이다.
게다가 KERNEL32 항목 중 상당수가 CRT 시작 코드가 부르는 것이고 게임 로직이 직접 쓰는
것이 아니다. 두 table의 차이 17개는 전부 보호 계층이 더한 것이며 9절에 정리한다.

*161 is **far smaller** than one might assume, which keeps the HLE scope for this project
tractable, and many KERNEL32 entries are called by CRT startup rather than by game logic.
All 17 entries by which the two tables differ are added by the protection layer; they are
listed in section 9.*

### 3rd·4th 원본 `.idata` 상세 / 3rd and 4th original `.idata` detail

| DLL | 3rd | 4th |
| --- | --- | --- |
| `KERNEL32.dll` | 88 | 88 |
| `USER32.dll` | 32 | 32 |
| `GDI32.dll` | 12 | 12 |
| `WINMM.dll` | 8 | 10 |
| `WS2_32.dll` | 9 | 9 |
| `AVIFIL32.dll` | 5 | 5 |
| `DDRAW.dll` | 2 | 2 |
| `ADVAPI32.dll` | 1 | 1 |
| `DSOUND.dll` | 1 | 1 |
| `DINPUT.dll` | 1 | 1 |
| 합계 | **159** | **161** |

1st SE 대비 `DINPUT.dll`, `AVIFIL32.dll`, `WS2_32.dll` 세 DLL이 늘고 `USER32` 표면이
21에서 32로 커진다. 버전별 HLE 프로파일이 필요한 이유가 이 차이다. 두 빌드의 그래픽
진입점은 `DirectDrawCreate`가 아니라 `DirectDrawCreateEx`다.

*Relative to 1st SE these builds add `DINPUT.dll`, `AVIFIL32.dll` and `WS2_32.dll`, and the
`USER32` surface grows from 21 to 32. This difference is why per-version HLE profiles are
needed. Both builds enter graphics through `DirectDrawCreateEx` rather than `DirectDrawCreate`.*

### 1st Tracks 원본 `.idata` 상세 / 1st Tracks original `.idata` detail

**확인됨 — 2026-09-15.** 1st Tracks 141개는 1st SE 144개의 **진부분집합**이다. 1st SE가 더
가진 이름은 `GDI32!BitBlt`, `GDI32!SetBkColor`, `KERNEL32!GetWindowsDirectoryA` 셋뿐이고,
1st Tracks에만 있는 이름은 없다.

*Confirmed — 2026-09-15. The 141 names of 1st Tracks form a **strict subset** of the 144 of
1st SE: 1st SE adds exactly `GDI32!BitBlt`, `GDI32!SetBkColor` and
`KERNEL32!GetWindowsDirectoryA`, and nothing appears only in 1st Tracks.*

| DLL | 1st Tracks | 1st SE |
| --- | --- | --- |
| `KERNEL32.dll` | 96 | 97 |
| `USER32.dll` | 21 | 21 |
| `GDI32.dll` | 12 | 14 |
| `WINMM.dll` | 8 | 8 |
| `DDRAW.dll` | 2 | 2 |
| `DSOUND.dll` | 1 | 1 |
| `ADVAPI32.dll` | 1 | 1 |
| 합계 / total | **141** | **144** |

두 제품 모두 `DirectDrawCreate`와 `DirectDrawEnumerateA`로 그래픽에 들어가며 `DINPUT`을
쓰지 않는다. 따라서 1st Tracks는 1st SE HLE 표면 안에 완전히 들어간다. 다만 표면이 같다는
것이 호출 순서와 인자 계약이 같다는 뜻은 아니다.

*Both products enter graphics through `DirectDrawCreate` and `DirectDrawEnumerateA` and use no
`DINPUT`, so 1st Tracks fits entirely inside the 1st SE HLE surface. An identical surface is
still not an identical call order or argument contract.*

### 5th 원본 `.idata` 상세 / 5th original `.idata` detail

**확인됨 — 2026-09-15.** 5th의 원본 `.idata`는 4th와 **DLL 목록과 함수 이름 집합이 완전히
같다.** 표에 나타나는 순서만 다르며, DLL별로 비교했을 때 한쪽에만 있는 이름은 없다. 따라서
5th는 4th 대비 새로운 Win32 API HLE를 요구하지 않는다. 개별 수치는 위 3rd·4th 표의 4th 열과
같다.

*Confirmed — 2026-09-15. The 5th original `.idata` has **exactly the same DLL list and function-name
set as 4th**; only the table order differs, and a per-DLL comparison finds no name present in
one and absent in the other. 5th therefore demands no Win32 API beyond what 4th already needs.
Its per-DLL numbers are the 4th column of the 3rd/4th table above.*

### 6th 상세 / 6th detail

**확인됨 — 2026-09-15.** 6th 게임 본체는 4th·5th 대비 `ADVAPI32`, `AVIFIL32`, `WS2_32` 세 DLL을
**전부** 뺀다. 즉 6th는 AVI 재생 경계도 Winsock 경계도 요구하지 않는다.

*Confirmed — 2026-09-15. Against 4th and 5th, the 6th game body drops `ADVAPI32`, `AVIFIL32` and
`WS2_32` **entirely**: 6th requires neither an AVI-playback boundary nor a Winsock boundary.*

| DLL | 4th·5th | 6th `EZ2DJ6th.EXE` | 6th 동봉 1st / bundled 1st | 6th bootstrap |
| --- | --- | --- | --- | --- |
| `KERNEL32.dll` | 88 | 87 | 95 | 65 |
| `USER32.dll` | 32 | 25 | 20 | 3 |
| `GDI32.dll` | 12 | 11 | 13 | — |
| `WINMM.dll` | 10 | 10 | 7 | — |
| `DDRAW.dll` | 2 | 2 | 2 | — |
| `DSOUND.dll` | 1 | 1 | 1 | — |
| `DINPUT.dll` | 1 | 1 | — | — |
| `ADVAPI32.dll` | 1 | — | — | — |
| `AVIFIL32.dll` | 5 | — | — | — |
| `WS2_32.dll` | 9 | — | — | — |
| 합계 / total | **161** | **137** | **138** | **68** |

6th가 새로 더하는 이름은 `KERNEL32!GetPrivateProfileIntA`, `KERNEL32!WritePrivateProfileStringA`,
`KERNEL32!GetFullPathNameA`, `KERNEL32!GetCurrentThread`, `GDI32!DeleteDC`다. 빼는 `USER32`
이름에는 `ChangeDisplaySettingsExA`, `EnumDisplaySettingsA`, `ExitWindowsEx`, `ReleaseDC`,
`SetCursor`, `RedrawWindow`, `DrawMenuBar`가 있다. bootstrap은 게임이 아니라 launcher이므로
`CreateProcessA`, `WaitForSingleObject`, `GetExitCodeProcess`, `SetPriorityClass`,
`TerminateProcess`, `SetCurrentDirectoryA`가 표면의 중심이고 그래픽·사운드 DLL을 쓰지 않는다.

*The names 6th adds are `KERNEL32!GetPrivateProfileIntA`, `KERNEL32!WritePrivateProfileStringA`,
`KERNEL32!GetFullPathNameA`, `KERNEL32!GetCurrentThread` and `GDI32!DeleteDC`. The `USER32`
names it drops include `ChangeDisplaySettingsExA`, `EnumDisplaySettingsA`, `ExitWindowsEx`,
`ReleaseDC`, `SetCursor`, `RedrawWindow` and `DrawMenuBar`. The bootstrap is a launcher rather
than a game, so its surface centers on `CreateProcessA`, `WaitForSingleObject`,
`GetExitCodeProcess`, `SetPriorityClass`, `TerminateProcess` and `SetCurrentDirectoryA`, and it
uses no graphics or sound DLL.*

**확인됨 — 2026-09-15.** 6th 동봉 1st Tracks 빌드(138)는 1999년 보호 빌드(141)와 다르다.
`ADVAPI32` 전체와 `USER32!ChangeDisplaySettingsExA`·`EnumDisplaySettingsA`·`ExitWindowsEx`·
`LoadImageA`가 빠지고, `KERNEL32!QueryPerformanceFrequency`, `GDI32!CreateDIBitmap`,
`USER32!GetDC`, `USER32!MessageBoxA`가 들어온다. display mode 변경 API가 빠진 것은 6th 캐비닛이
mode 전환을 launcher 쪽에서 처리하거나 아예 하지 않는다는 뜻일 수 있으나, 이는 **추정**이며
런타임 관찰로 확인해야 한다.

*Confirmed — 2026-09-15. The 6th's bundled 1st Tracks build (138) differs from the 1999 protected
build (141): it drops all of `ADVAPI32` plus `USER32!ChangeDisplaySettingsExA`,
`EnumDisplaySettingsA`, `ExitWindowsEx` and `LoadImageA`, and adds
`KERNEL32!QueryPerformanceFrequency`, `GDI32!CreateDIBitmap`, `USER32!GetDC` and
`USER32!MessageBoxA`. The absent display-mode APIs may mean the 6th cabinet handles mode
switching in the launcher or not at all, but that is **inferred** and needs runtime
confirmation.*

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

**확인됨 — 정정됨, 2026-08-25.** import table에는 Direct3D 이름이 없지만, 이것은 Direct3D 미사용을 뜻하지 않는다. 보호 해제 후 실행 추적에서 `DirectDrawCreate`로 만든 객체가 `IDirectDraw4`와 `IDirect3D3`를 제공했고, 원본은 `IDirect3D3::FindDevice`를 hardware-only 조건으로 호출했다. Direct3D는 별도 DLL import가 아니라 DirectDraw COM의 `QueryInterface` 경로로 도달한다.

**확인됨.** GDI DIB와 DirectDraw surface 호출도 존재하고 `ChangeDisplaySettingsExA`는 640×480×16 전체 화면 mode를 요청한다. 그러나 현재 증거만으로 최종 화면이 2D blit만으로 구성된다고 확정할 수 없다. 실제 Direct3D 3 method, surface format과 primitive 범위는 후속 COM trace로 누적한다.

*Confirmed — corrected on 2026-08-25. The import table has no function named Direct3D, but that does not mean Direct3D is unused. Post-unprotection runtime traces show the object created by `DirectDrawCreate` providing `IDirectDraw4` and `IDirect3D3`, followed by a hardware-only `IDirect3D3::FindDevice` call. Direct3D is reached through DirectDraw COM `QueryInterface`, not a separate DLL import. GDI DIB, DirectDraw surfaces, and a 640×480×16 display request also exist, but current evidence does not prove that final rendering is exclusively 2D blitting. The actual Direct3D 3 methods, surface formats, and primitive set remain subject to COM tracing.*

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

**확인됨 — 2026-08-23, 2026-09-14 재측정.** 보호된 `ez2dj.exe`의 import directory는
`.gidata`(RVA `0x01ad8000`)로 옮겨져 있으며, 내용은 원본 `.idata`(RVA `0x01aba000`) 표면
144개 전체에 보호 특화 API 17개를 더한 161개다. 두 table을 같은 파일 안에서 각각 해석해
차집합을 구했으므로, 어떤 이름이 보호 계층의 것인지 이름 단위로 확정된다.

보호 계층이 추가한 import 17개 — 이것이 `.gidata`와 `.idata`의 차집합 전체다.

```text
KERNEL32.dll  DeviceIoControl, CreateMutexA, ReleaseMutex, FreeLibrary,
              OpenFile, _lopen, _lread, _lwrite, _lclose, _lcreat, _llseek,
              DeleteFileA, MoveFileA, lstrcmpiA, lstrcpyA, lstrlenA
USER32.dll    MessageBoxA
```

**정정 — 2026-09-14.** 이전 판은 `DebugBreak`, `OutputDebugStringA`, `FatalAppExitA`,
`UnhandledExceptionFilter`, `RaiseException`, `RtlUnwind`, `IsBadReadPtr`, `IsBadWritePtr`,
`HeapValidate` 아홉 개도 보호 계층이 추가한 것으로 적었다. 재측정 결과 아홉 개 모두 원본
`.idata`에 이미 있는 MSVC CRT import이며 보호 계층과 무관하다.

`DeviceIoControl`이 보호 계층 전용이라는 점은 이 제품군의 동글 경계와 직접 이어진다. 게임
본체는 장치 IOCTL을 하지 않으며, `\.\LPTDI1`·`\.\FEnteDev`·`\.\NTICE` 경로의 IOCTL
왕복은 전부 보호 계층의 것이다.

`.gdata`에는 `\.\TDSD.VXD`, `\.\LPTDI0`, `MSVBVM50.DLL`(2회), EUC-KR 메시지 바이트,
해시성 blob이 있다. 런타임 관찰([HDD 레이아웃 분석](ez2dj-hdd-layout.md))에서
`CreateFileA("\.\LPTDI1")` 병렬포트 열기와 `GetProcAddress(wsock32, "WSAGetLastError")`가
확인됐다. 즉 보호 계층의 동적 해석은 최소화되어 있고(관찰된 것은 `WSAGetLastError` 하나),
하드웨어·환경 검사는 정적 import와 문자열로 구성된다.

*Confirmed — 2026-08-23, re-measured 2026-09-14. The protected `ez2dj.exe` moves its import
directory into `.gidata` (RVA 0x01ad8000), holding 161 entries: the complete 144-entry
original `.idata` surface (RVA 0x01aba000) plus 17 protection-specific APIs. Both tables were
parsed inside the same file and differenced, so the protection-layer names are established
individually: `DeviceIoControl`, `CreateMutexA`, `ReleaseMutex`, `FreeLibrary`, the
`OpenFile`/`_l*` legacy file family, `DeleteFileA`, `MoveFileA`, `lstrcmpiA`, `lstrcpyA`,
`lstrlenA`, and `MessageBoxA`.*

*Corrected — 2026-09-14. An earlier revision also attributed `DebugBreak`,
`OutputDebugStringA`, `FatalAppExitA`, `UnhandledExceptionFilter`, `RaiseException`,
`RtlUnwind`, `IsBadReadPtr`, `IsBadWritePtr` and `HeapValidate` to the protection layer.
Re-measurement shows all nine are MSVC CRT imports already present in the original `.idata`
and unrelated to the protection layer.*

*That `DeviceIoControl` belongs only to the protection layer connects directly to this
product family's dongle boundary: the game body issues no device IOCTL, and all IOCTL
round trips over `\.\LPTDI1`, `\.\FEnteDev` and `\.\NTICE` belong to the protection layer.*

*`.gdata` holds `\.\TDSD.VXD`, `\.\LPTDI0`, `MSVBVM50.DLL` (twice), EUC-KR message bytes,
and hash-like blobs. Runtime observation (see the HDD layout analysis) confirms a
`CreateFileA("\.\LPTDI1")` parallel-port open and `GetProcAddress` for only
`WSAGetLastError`, so dynamic resolution is minimal and hardware/environment checks are built
from static imports plus strings.*

---

## 10. 미확정 / Unresolved

* 보호된 `ez2dj.exe`와 `EZ2DJ.EXE`가 런타임에 추가로 해석하는 API. 스텁이 `GetProcAddress`로 가져오므로 정적으로는 보이지 않는다. → 1st SE는 관찰상 `WSAGetLastError` 하나뿐. 다른 버전은 미확정.
* ~~`CreateFileA`가 실제로 여는 경로~~ → **확정됨**: 보호 stub이 `\\.\LPTDI1`을 연다. 게임 본체의 파일 경로는 여전히 미확정.
* 스레드 개수와 역할.
* DirectDraw 표면 구성: 표면 개수, 픽셀 포맷, 해상도.

*Unresolved: run-time `GetProcAddress` additions by the protected builds — 1st SE observes only WSAGetLastError; other versions are unresolved. The `CreateFileA` path question is now resolved for the protection stub (`\\.\LPTDI1`) but not for the game body. Still open: thread count and roles, and the DirectDraw surface configuration.*

## 11. 확인됨: 4th Trax 정적 import와 첫 동적 해석 / Confirmed: 4th Trax static imports and first dynamic resolutions

**확인됨 — 2026-09-01.** 사용자 제공 <code>4thTrax.chd</code>에서
<code>EZ2DJ/EZ2DJ.EXE</code>를 <code>Fat32Volume</code>으로 읽고
<code>re2dj_pe_loader</code>로 import directory를 해석한 결과, 다음 36개 import가
확인됐다. 이 목록은 4th 실행 파일에 대한 것이며 1st SE의 144개 목록을 대신하지
않는다.

~~~text
KERNEL32.dll  lstrcmpA, CloseHandle, CreateFileA, LocalFree, LocalAlloc,
              SetErrorMode, GetProcAddress, GetCurrentProcessId,
              GetEnvironmentVariableA, GetModuleHandleA, FreeLibrary, LoadLibraryA,
              lstrlenA, GetVersion, ReadFile, WriteFile, GetFileSize,
              FindFirstFileA, FindNextFileA, FindClose, GetModuleFileNameA,
              GetLocalTime, SystemTimeToFileTime, GetSystemTime, Sleep
USER32.dll    MessageBoxA, UpdateWindow
GDI32.dll     GetStockObject
ADVAPI32.dll  RegFlushKey
WINMM.dll     mixerGetLineControlsA
DSOUND.dll    #1
DINPUT.dll    DirectInputCreateA
DDRAW.dll     DirectDrawCreateEx
AVIFIL32.dll  AVIStreamInfoA
WS2_32.dll    #9
~~~

**확인됨 — 2026-09-01 bounded API trace.** <code>--api-trace --trace</code>는
4th staging executable에서 entry 이후 다음 동적 해석을 순서대로 기록했다.

1. <code>GetProcAddress(kernel32, "GetVersion")</code>, caller <code>0x00af0b99</code>
2. <code>GetProcAddress(kernel32, "CreateFileA")</code>, caller <code>0x00af09f6</code>

그 뒤 trace child는 <code>0xc0000005</code> execute fault로 종료했으며, CHD VFS를
활성화한 기존 <code>--run</code>에서는 첫 <code>CreateFileA</code> runtime handoff가
bounded timeout 안에 관찰되지 않았다. 이는 **확인됨**인 API 순서와 실행 종료
사실이지만, fault의 직접 원인과 정상적인 <code>CreateFileA</code> 반환 경로는 아직
**미확정**이다.

*Confirmed — 2026-09-01. Reading <code>EZ2DJ/EZ2DJ.EXE</code> from the
user-supplied <code>4thTrax.chd</code> through <code>Fat32Volume</code> and parsing
its import directory with <code>re2dj_pe_loader</code> found the 36 imports listed
above. They belong to the 4th executable and do not replace the 1st SE list of 144
imports.*

*Confirmed — 2026-09-01 bounded API trace. <code>--api-trace --trace</code> recorded
these post-entry dynamic resolutions in order: <code>GetProcAddress(kernel32,
"GetVersion")</code> from caller <code>0x00af0b99</code>, then
<code>GetProcAddress(kernel32, "CreateFileA")</code> from caller
<code>0x00af09f6</code>. The trace child then exited with an
<code>0xc0000005</code> execute fault, while the existing CHD-VFS <code>--run</code>
did not observe the first <code>CreateFileA</code> runtime handoff within its bound.
The API order and exit are confirmed; the direct fault cause and normal
<code>CreateFileA</code> return path are unresolved.*

## 12. 확인됨: 4th 동적 VFS resolver 경계 / Confirmed: 4th dynamic VFS resolver boundary

**확인됨 — 2026-09-01 작업 119.** 4th profile의
<code>hle_dynamic_vfs</code> capability를 활성화한 Windows x86 launcher는
원본 <code>GetProcAddress</code> IAT 2개를 injected runtime thunk로 연결하고,
진단 JSONL에 다음 event를 기록했다.

~~~text
{"event":"vfs_dynamic_resolver","enabled":true,"slots":2}
~~~

같은 실행의 VFS trace에는 <code>GetVersion</code>은
<code>route=win32</code>, <code>CreateFileA</code>는
<code>route=hle</code>로 기록됐다. 따라서 4th 동적 resolver 결과를 VFS
wrapper로 전달하는 연결은 **확인됨**이다. 그러나 이 실행은 asset-open event
전에 <code>0xc0000005</code> execute fault로 종료했으므로 첫 실제 게임 파일
open, 보호 응답, 정상 실행은 **미확정**이다. runtime diagnostic의
<code>outcome=status=success</code>는 bounded observation 경계 도달을 뜻하며
게임 실행 성공을 뜻하지 않는다.

*Confirmed — 2026-09-01 task 119. With the 4th profile's
<code>hle_dynamic_vfs</code> capability enabled, the Windows x86 launcher
patched two original <code>GetProcAddress</code> IAT slots to the injected
runtime thunk and recorded this diagnostic event:

~~~text
{"event":"vfs_dynamic_resolver","enabled":true,"slots":2}
~~~

The VFS trace from the same run recorded <code>GetVersion</code> with
<code>route=win32</code> and <code>CreateFileA</code> with
<code>route=hle</code>. The routing of the 4th dynamic resolver result through
the VFS wrapper is therefore **confirmed**. The child nevertheless exited with
an <code>0xc0000005</code> execute fault before an asset-open event, so the first
real game-file open, protection response, and normal execution remain
**unresolved**. The runtime diagnostic <code>outcome=status=success</code> means
only that the bounded observation boundary was reached; it does not mean that
the game executed successfully.*

## 15. 확인됨: resolver caller의 반환값 저장 / Confirmed: resolver-caller return-value store

**확인됨 — 2026-09-01 작업 122.** runtime memory에서
<code>CreateFileA</code> resolver caller window
<code>base=0x00af09ee</code>, <code>caller=0x00af09f6</code>가
<code>readable=1</code>로 읽혔다. caller offset 8의 bytes
<code>89 45 dc</code>는 resolver가 반환한 EAX를
<code>[EBP-0x24]</code>에 저장하는 instruction으로 해석된다.

따라서 반환값은 즉시 wrapper를 호출하는 대신 protected stack-local 위치에
저장되는 단계까지 **확인됨**이다. 저장된 pointer의 후속 consumer, indirect
call 여부, 첫 파일 open과 보호 응답은 **미확정**이다. 이 관찰은 실행 중
child memory에서 읽은 bytes이며 디스크의 protected section을 정적 해석한
결과가 아니다.

*Confirmed — 2026-09-01 task 122. The runtime memory window for the
<code>CreateFileA</code> resolver caller was readable at
<code>base=0x00af09ee</code> and <code>caller=0x00af09f6</code>. The bytes at
caller offset 8, <code>89 45 dc</code>, decode as an instruction storing the
resolver's EAX result at <code>[EBP-0x24]</code>.*

*The result is therefore confirmed to reach a protected stack-local store
rather than an immediately observed wrapper call. The later consumer of that
stored pointer, any indirect call, the first file open, and the protection
response remain **unresolved**. The bytes were read from child memory during
execution; this is not a static interpretation of the on-disk protected
section.*

## 13. 확인됨: 동적 resolver와 VFS wrapper 호출의 분리 / Confirmed: resolver selection versus VFS-wrapper call

**확인됨 — 2026-09-01 작업 120.** 4th dynamic VFS bounded trace의
<code>.vfs.log</code>에는 작업 119와 같이
<code>CreateFileA:route=hle</code>가 남았지만, 새로 추가한
<code>Re2djVfsCreateFileA</code> bounded request event
<code>create-file:stage=request</code>는 기록되지 않았다. 따라서 resolver가
HLE 함수 주소를 선택한 사실과 protected stub이 반환 주소를 실제 호출한
사실은 분리해야 한다. 현재 실제 wrapper 호출, CHD pseudo-handle, overlay/native
open 결과는 **미확정**이다.

*Confirmed — 2026-09-01 task 120. The 4th dynamic-VFS bounded trace retained
<code>CreateFileA:route=hle</code> in the VFS log, but the new bounded request
event <code>create-file:stage=request</code> from
<code>Re2djVfsCreateFileA</code> was absent. Resolver selection of an HLE
function address must therefore be kept separate from evidence that the
protected stub called the returned address. The actual wrapper call, CHD
pseudo-handle, and overlay/native open result remain **unresolved**.*

## 14. 확인됨: 동적 resolver 반환 주소 / Confirmed: dynamic resolver return addresses

**확인됨 — 2026-09-01 작업 121.** 실제 4th CHD VFS log는 다음 반환값과
caller를 기록했다.

~~~text
GetVersion:route=win32:address=0x77451c10:caller=0x00af0b99
CreateFileA:route=hle:address=0x62f5350d:caller=0x00af09f6
~~~

launcher diagnostic의 kernel32 base는 <code>0x77430000</code>, injected
runtime base는 <code>0x62f50000</code>였으므로 두 반환 주소가 각각 기대한
module 범위에 있는 것은 **확인됨**이다. 그러나
<code>create-file:stage=request</code>가 계속 없고 child가
<code>eip=0x00000000</code> execute fault로 종료했으므로 returned pointer의
실제 호출, 정확한 ABI 호환, 첫 파일 open과 보호 응답은 **미확정**이다.

*Confirmed — 2026-09-01 task 121. The real 4th CHD VFS log recorded:

~~~text
GetVersion:route=win32:address=0x77451c10:caller=0x00af0b99
CreateFileA:route=hle:address=0x62f5350d:caller=0x00af09f6
~~~

The launcher diagnostic reported kernel32 base
<code>0x77430000</code> and injected-runtime base
<code>0x62f50000</code>, confirming that both returned addresses lie in their
expected module ranges. The
<code>create-file:stage=request</code> event is still absent and the child
exited with an execute fault at <code>eip=0x00000000</code>. Actual invocation
of the returned pointer, exact ABI compatibility, the first file open, and the
protection response remain **unresolved**.*

## 16. 확인됨: EIP=0 indirect call fault 경계 / Confirmed: EIP=0 indirect-call fault boundary

**확인됨 — 2026-09-01 작업 123.** fault stack의 첫 return address는
<code>0x00aef7fe</code>였고, live child memory에서 그 직전 bytes
<code>ff15f40caf0083c4</code>가 읽혔습니다. 이는
<code>CALL DWORD PTR [0x00AF0CF4]</code>와 그 다음 정리 명령의 시작에
해당합니다. <code>0x00AF0CF4</code> pointer slot 자체는 readable했지만
값은 <code>0x00000000</code>였고, 별도의 target allocation이나 image
section은 기록되지 않았습니다.

따라서 이 실행은 <code>EIP=0</code> execute fault가 zero-pointer
indirect call 직후 발생한 경계를 **확인**합니다. 그러나 pointer slot이
0으로 남은 원인과 이것이 동적 <code>CreateFileA</code> 반환값 소비 실패인지,
보호 코드의 다른 초기화 경로인지, 실제 HLE ABI 결함인지는 **미확정**입니다.
정상적인 VFS open과 보호 응답도 여전히 확인되지 않았습니다.

*Confirmed — 2026-09-01 task 123. The first fault-stack return address was
<code>0x00aef7fe</code>, and live child memory contained
<code>ff15f40caf0083c4</code> immediately before it. This corresponds to
<code>CALL DWORD PTR [0x00AF0CF4]</code> followed by the start of the cleanup
instruction. The pointer slot at <code>0x00AF0CF4</code> was readable but held
<code>0x00000000</code>; no target allocation or image section was therefore
recorded.

This run consequently confirms a zero-pointer indirect-call boundary
immediately before the <code>EIP=0</code> execute fault. The reason the slot is
zero, whether it is the dynamic <code>CreateFileA</code> result consumer or a
different protected initialization path, and whether an HLE ABI defect is
involved remain **unresolved**. Normal VFS opening and a protection response
are still unconfirmed.*

## 17. 확인됨: private pointer slot 참조 표면 / Confirmed: private pointer-slot reference surface

**확인됨 — 2026-09-01 작업 124.** <code>dumpbin /imports</code>로 확인한
정식 IAT 범위에는 <code>0x00AF0CF4</code>가 포함되지 않습니다. injected
runtime과 CHD VFS를 사용하지 않은 native baseline에서도 native
<code>CreateFileA</code> 주소 <code>0x774533A0</code>가 반환된 뒤 해당 slot은
0이었고 동일한 execute fault가 발생했습니다. 따라서 이 slot은 현재 동적
VFS HLE가 직접 생성한 IAT entry가 아니라 보호 section 내부의 private
pointer table 또는 상태 위치로 분류합니다.

fault 시점 live main image의 7,446,528 bytes를 검사해 12개 참조를
확인했습니다. 명령 경계가 분명한 참조는 다음과 같습니다.

| 분류 | 명령 주소 |
|---|---|
| <code>MOV [slot], EAX</code> | <code>0x00AEF5F0</code>, <code>0x00AEFE62</code>, <code>0x00AF061A</code> |
| <code>CALL DWORD PTR [slot]</code> | <code>0x00AEF5C8</code>, <code>0x00AEF7F8</code>, <code>0x00AEFD68</code>, <code>0x00AEFFE7</code>, <code>0x00AF0500</code>, <code>0x00AF06A4</code> |
| <code>CMP DWORD PTR [slot], 0</code> | <code>0x00AEF645</code>, <code>0x00AEF90F</code>, <code>0x00AF0954</code> |

writer 명령의 존재는 **확인됨**이지만 실행 여부와 실행 시 EAX 값은
**미확정**입니다. 따라서 아직 특정 writer path 누락이나
<code>CreateFileA</code> 반환값과의 동일성을 확정하지 않습니다.

*Confirmed — 2026-09-01 task 124. The formal IAT ranges reported by
<code>dumpbin /imports</code> do not include <code>0x00AF0CF4</code>. A native
baseline without the injected runtime or CHD VFS returned native
<code>CreateFileA</code> at <code>0x774533A0</code>, yet the slot remained zero
and the same execute fault occurred. The slot is therefore classified as a
private pointer table or state location inside the protected section rather
than an IAT entry directly created by the current dynamic-VFS HLE.

A scan of 7,446,528 live main-image bytes at fault time found 12 references.
References with clear instruction boundaries are:

| Class | Instruction addresses |
|---|---|
| <code>MOV [slot], EAX</code> | <code>0x00AEF5F0</code>, <code>0x00AEFE62</code>, <code>0x00AF061A</code> |
| <code>CALL DWORD PTR [slot]</code> | <code>0x00AEF5C8</code>, <code>0x00AEF7F8</code>, <code>0x00AEFD68</code>, <code>0x00AEFFE7</code>, <code>0x00AF0500</code>, <code>0x00AF06A4</code> |
| <code>CMP DWORD PTR [slot], 0</code> | <code>0x00AEF645</code>, <code>0x00AEF90F</code>, <code>0x00AF0954</code> |

The writer instructions are **confirmed to exist**, but their execution and
the EAX value at execution remain **unresolved**. No writer-path omission or
identity with the dynamic <code>CreateFileA</code> result is asserted yet.*

## 18. 확인됨: writer 실행과 첫 장치 open / Confirmed: writer execution and first device opens

**확인됨 — 2026-09-01 작업 125.** broad API software watch가 없는 실제
CHD/VFS trace에서 <code>0x00AEFE62</code>의
<code>MOV [0x00AF0CF4], EAX</code>가 실행됐습니다. 실행 직전 EAX는
<code>0x00B17B00</code>, slot은 0이었고, bounded trace 종료 시 slot은
<code>0x00B17B00</code>이었습니다. HLE 없는 native baseline에서도 같은
writer와 EAX가 확인됐으므로 이 writer 선택과 값 자체는 현재 VFS HLE가 만든
것이 아닙니다.

writer 이후 실제 VFS wrapper request는 다음 순서로 확인됐습니다.

```text
\\.\NTICE    access=0xc0000000 disposition=3 -> unmapped, error=123
\\.\NTICE    access=0xc0000000 disposition=3 -> unmapped, error=123
\\.\FEnteDev access=0xc0000000 disposition=3 -> unmapped, error=123
```

따라서 resolver 주소 선택뿐 아니라 실제 <code>CreateFileA</code> wrapper
진입이 **확인됨**이며, 첫 보호 경계는 asset file이 아니라 NTICE/FEnteDev
device open입니다. 현재 VFS가 두 Win32 device path를 일반 guest path로
해석해 error 123을 반환하므로, 다음 미확정 범위는 device handle과 후속
<code>DeviceIoControl</code> protocol입니다.

같은 실행에 broad API trace의 40개 software watch를 적용한 대조군은 writer
hit 0회, slot 0, <code>EIP=0</code> fault로 끝났습니다. broad API trace는 이
보호 continuation의 투명한 관찰 수단이 아니며, 정확히 어느 watch가 분기를
바꾸는지는 **미확정**입니다.

*Confirmed — 2026-09-01 task 125. In the real CHD/VFS trace without broad API
software watches, <code>MOV [0x00AF0CF4], EAX</code> at
<code>0x00AEFE62</code> executed with EAX <code>0x00B17B00</code> and a zero
pre-store slot. The slot held <code>0x00B17B00</code> at the bounded trace
boundary. A native baseline without HLE produced the same writer and EAX, so
the current VFS HLE did not create that writer selection or value.

Actual VFS wrapper requests after the writer were:

```text
\\.\NTICE    access=0xc0000000 disposition=3 -> unmapped, error=123
\\.\NTICE    access=0xc0000000 disposition=3 -> unmapped, error=123
\\.\FEnteDev access=0xc0000000 disposition=3 -> unmapped, error=123
```

This **confirms** actual <code>CreateFileA</code> wrapper entry, not merely
resolver address selection. The first protection boundary is an NTICE/FEnteDev
device open rather than an asset file. The VFS currently interprets both
Win32 device paths as ordinary guest paths and returns error 123, leaving
device handles and the later <code>DeviceIoControl</code> protocol unresolved.

A control run with all 40 broad API software watches had zero writer hits, a
zero slot, and the <code>EIP=0</code> fault. Broad API tracing is not transparent
for this protected continuation; which watch changes the branch remains
**unresolved**.*
