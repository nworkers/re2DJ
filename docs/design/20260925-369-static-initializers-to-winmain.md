# 작업 369 설계 — 정적 초기화에서 WinMain까지 / Task 369 design — From static initializers to WinMain

선행: [작업 368 설계](20260925-368-original-crt-startup.md)

## 배경 / Background

작업 368 뒤 실제 4th는 게임 코드의 첫 호출 `CreateEventA`에서 멈췄다. 작업 368처럼 멈추는 곳마다 구현하며 따라가 다음 흐름을 확인했다.

*After Task 368 the real 4th stopped at the game code's first call, `CreateEventA`. Implementing each stop as in Task 368 established this flow:*

1. **C++ 정적 초기화**(`0x00406fe3` 등). `CreateEventA(NULL, FALSE, FALSE, NULL)`를 부르고, `VirtualAlloc`으로 8 MiB, 256 KiB, 1 MiB를 잡는다. CRT heap의 `HeapSize`도 여러 번 부른다.
   ***C++ static initialization** (`0x00406fe3` and others): `CreateEventA(NULL, FALSE, FALSE, NULL)`, `VirtualAlloc` of 8 MiB, 256 KiB, and 1 MiB, and repeated `HeapSize` on the CRT heap.*
2. **게임에 link된 Hardlock API**(`0x004b…`). envelope와 같은 순서로 로그인한다. `HL_SEARCH` → advapi32 → `GetVersionExA` → WTS 세션 → `wfapi.dll` → `IsTNT`/`Borland32` → `\\.\FEnteDev` → handshake 2 → descriptor 1.
   ***The Hardlock API linked into the game** (`0x004b…`) logs in in the envelope's order: `HL_SEARCH` → advapi32 → `GetVersionExA` → WTS session → `wfapi.dll` → `IsTNT`/`Borland32` → `\\.\FEnteDev` → two handshakes → one descriptor.*
3. **CRT 시간 초기화.** `GetLocalTime` → `GetSystemTime` → `GetTimeZoneInformation` → `WideCharToMultiByte`(zone 이름) 순서다.
   ***CRT time set-up:** `GetLocalTime` → `GetSystemTime` → `GetTimeZoneInformation` → `WideCharToMultiByte` (zone names).*
4. 두 번째 `CreateEventA(…, bInitialState = TRUE, …)`를 부른다.
   *A second `CreateEventA(…, bInitialState = TRUE, …)`.*
5. **WinMain 진입.** `GetStartupInfoA`와 `GetModuleHandleA(NULL)`을 부른 뒤, 첫 호출은 `winmm!timeBeginPeriod(1)`(`0x00406cdc`)이다.
   ***WinMain entry:** `GetStartupInfoA` and `GetModuleHandleA(NULL)`, then the first call, `winmm!timeBeginPeriod(1)` (`0x00406cdc`).*

## 결정 / Decisions

1. **event.** `GuestProcess`가 이름 없는 event를 공용 handle 공간에 둔다. 속성은 manual/auto reset과 signal 여부다. 함수별 동작은 다음과 같다.
   ***Events:** `GuestProcess` keeps unnamed events in the shared handle space, with manual/auto reset and signal state. Per function:*
   - `CreateEventA`: 이름이 있으면 handler 실패로 멈춘다. process 사이 공유는 다루지 않기 때문이다.
     *`CreateEventA`: a name stops the handler, since sharing across processes is not modelled.*
   - `SetEvent`/`ResetEvent`: signal 상태를 바꾼다. 모르는 handle이면 `ERROR_INVALID_HANDLE`이다.
     *`SetEvent`/`ResetEvent` change the signal state, with `ERROR_INVALID_HANDLE` for an unknown handle.*
   - `WaitForSingleObject`(event): signal 상태면 `WAIT_OBJECT_0`이고, auto reset event는 이때 reset된다. signal이 아니고 timeout이 0이면 `WAIT_TIMEOUT`이다. 모르는 handle이면 `WAIT_FAILED`다. 막히는 대기는 게스트 thread가 하나뿐이라 풀릴 수 없으므로 멈춘다.
     *`WaitForSingleObject` (events): `WAIT_OBJECT_0` when signalled, resetting an auto-reset event; `WAIT_TIMEOUT` when unsignalled with a zero timeout; `WAIT_FAILED` for an unknown handle. A blocking wait stops, since with a single guest thread it could never be released.*
   - `CloseHandle`: event도 닫는다.
     *`CloseHandle` closes events too.*
2. **시계 서비스.** `ImportCallServices::ReadClock`이 `GuestClockReading`을 준다. 구성은 UTC FILETIME, 현지 시각에서 UTC를 뺀 분 단위 차, 단조 증가 ms다. Linux는 `CLOCK_REALTIME`, `localtime_r`의 `tm_gmtoff`, `CLOCK_MONOTONIC`을 쓴다. 공용 `win32_time`이 FILETIME↔SYSTEMTIME을 바꾼다(proleptic Gregorian, 1601-01-01은 월요일).
   ***Clock service:** `ImportCallServices::ReadClock` gives a `GuestClockReading` — UTC FILETIME, local minus UTC in minutes, and monotonic milliseconds; Linux uses `CLOCK_REALTIME`, `localtime_r`'s `tm_gmtoff`, and `CLOCK_MONOTONIC`. The shared `win32_time` converts FILETIME↔SYSTEMTIME (proleptic Gregorian; 1601-01-01 was a Monday).*
   - `GetSystemTime`, `GetLocalTime`(host의 현지 차를 더함), `SystemTimeToFileTime`(범위 밖 field는 `ERROR_INVALID_PARAMETER`), `GetTickCount`.
     *`GetSystemTime`, `GetLocalTime` (adding the host's local offset), `SystemTimeToFileTime` (`ERROR_INVALID_PARAMETER` for fields out of range), and `GetTickCount`.*
   - `GetTimeZoneInformation`: bias는 현재 현지 차의 음수다. 일광 절약 규칙은 없고 `TIME_ZONE_ID_UNKNOWN`을 돌려준다. zone 이름은 비운다. 한국어 Windows는 zone 이름이 한글인데, 게스트가 이를 CP949 두 byte로 바꿔야 하고 그 변환은 아직 없다. CRT는 이 이름을 `_tzname`에만 쓴다.
     *`GetTimeZoneInformation`: the bias is the current local offset negated, with no daylight rule (`TIME_ZONE_ID_UNKNOWN`) and empty zone names. A Korean Windows names its zone in Hangul, which the guest would convert as double-byte CP949, not modelled yet; the CRT uses the names only for `_tzname`.*

## 기대 결과 / Expected result

두 Linux 폭에서 실제 4th가 WinMain에 들어간다. 첫 호출 `timeBeginPeriod`에서 멈춘다.

*On both Linux widths the real 4th enters WinMain and stops at its first call, `timeBeginPeriod`.*

## 범위 밖 / Out of scope

- WinMain 안의 winmm, window, DirectX. / *winmm, windows, and DirectX inside WinMain.*
- guest thread와 막히는 대기, 이름 있는 event. / *Guest threads, blocking waits, and named events.*
- 일광 절약 규칙과 zone 이름. / *Daylight rules and zone names.*
