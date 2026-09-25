# 작업 369 작업 로그 — 정적 초기화에서 WinMain까지 / Task 369 work log — From static initializers to WinMain

설계: [20260925-369-static-initializers-to-winmain.md](../design/20260925-369-static-initializers-to-winmain.md)
작업 지시서: [20260925-369-static-initializers-to-winmain.md](../work-orders/20260925-369-static-initializers-to-winmain.md)

## 변경 / Changes

- **`GuestProcess`**: `GuestEvent`, `CreateEvent`, `FindEvent`, `CloseEvent`.
- **`win32_time.h/.cpp`**(새 파일): FILETIME↔SYSTEMTIME 변환과 SYSTEMTIME byte 배치.
  ***`win32_time.h/.cpp`** (new): FILETIME↔SYSTEMTIME conversion and the SYSTEMTIME byte layout.*
- **`ImportCallServices::ReadClock`**과 `GuestClockReading`. Linux 진단이 host 시계로 구현한다.
  ***`ImportCallServices::ReadClock`** with `GuestClockReading`, implemented by the Linux diagnostic from the host clock.*
- **kernel32**: `CreateEventA`, `SetEvent`, `ResetEvent`, `WaitForSingleObject`, `GetTickCount`, `GetSystemTime`, `GetLocalTime`, `SystemTimeToFileTime`, `GetTimeZoneInformation`을 구현했다. 구현 60개, 해석 전용 37개다. `CloseHandle`은 event도 닫는다.
  ***kernel32** implements `CreateEventA`, `SetEvent`, `ResetEvent`, `WaitForSingleObject`, `GetTickCount`, `GetSystemTime`, `GetLocalTime`, `SystemTimeToFileTime`, and `GetTimeZoneInformation`, for 60 implemented and 37 resolve-only exports; `CloseHandle` closes events.*
- **단위 테스트**(`kernel32_crt_test.cpp`):
  ***Unit tests** (`kernel32_crt_test.cpp`):*
  - event: auto·manual reset, timeout 0, 막히는 대기 정지, 닫힌 handle, 이름 거절. / *events: auto and manual reset, zero timeout, the blocking-wait stop, closed handles, and name rejection;*
  - 시간 변환: 1601·1970 기준점, 2024-02-29 왕복, 범위 밖. / *time conversion: the 1601 and 1970 origins, a 2024-02-29 round trip, out-of-range fields;*
  - 고정 시계의 시간 export(시계가 없으면 정지, UTC/현지, 요일, `SystemTimeToFileTime`, 시간대 bias −540). / *the time exports on a fixed clock: stopping without a clock, UTC and local, the weekday, `SystemTimeToFileTime`, and the −540 zone bias.*
  - 기존 kernel32 테스트는 해석 전용 예시를 `GetTickCount`에서 `Sleep`으로 바꿨다. / *The kernel32 test's resolve-only example moves from `GetTickCount` to `Sleep`.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음(trace 43 frame) / as before (43-frame trace) |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 호출 1,700번, 주소를 정규화하면 같음. Hardlock은 total 79(initialize 1, handshake 4, descriptor 38, transform 36). `#1698 GetStartupInfoA`와 `#1699 GetModuleHandleA(NULL)` 뒤 `#1700 timeBeginPeriod(1)`에서 정지 / 1,700 calls, identical after address normalization; Hardlock total 79 (initialize 1, handshake 4, descriptor 38, transform 36); after `#1698 GetStartupInfoA` and `#1699 GetModuleHandleA(NULL)` the run stops at `#1700 timeBeginPeriod(1)` |

## 다음 / Next

WinMain 안의 winmm 시간 함수(`timeBeginPeriod`, `timeGetTime`)부터 시작한다. 그다음 window class·window 생성과 message loop, DirectX로 간다.

*Start with WinMain's winmm timing (`timeBeginPeriod`, `timeGetTime`), then the window class, window creation, and the message loop, then DirectX.*
