# 작업 361 작업 로그 — 게스트 장치 handle과 `kernel32` 장치 호출 / Task 361 work log — Guest device handles and `kernel32` device calls

설계: [20260924-361-linux-guest-device-handles.md](../design/20260924-361-linux-guest-device-handles.md)
작업 지시서: [20260924-361-linux-guest-device-handles.md](../work-orders/20260924-361-linux-guest-device-handles.md)

## 변경 / Changes

- **`hle::GuestDeviceSet`**(`include/re2dj/hle/guest_devices.h`)을 추가했다. 프로파일의 장치 prefix와 맞는 이름을 열고, handle(`0x1004`부터 4씩)을 관리한다. 열린 handle의 요청은 작업 360의 `CompleteHardlockDeviceIoControl`로 처리하고 통계를 남긴다.
  *Added **`hle::GuestDeviceSet`** (`include/re2dj/hle/guest_devices.h`): it opens names matching the profile's device prefix, manages handles (from `0x1004` in steps of four), and answers requests on open handles through Task 360's `CompleteHardlockDeviceIoControl` with request counts.*
- **`hle/win32_errors.h`**에 HLE가 쓰는 Win32 오류 코드를 모았다. `device_call.h`도 이것을 쓴다.
  *Collected the Win32 error codes HLE uses in **`hle/win32_errors.h`**, used by `device_call.h` as well.*
- **`ImportCallServices`**에 선택 서비스(`ReadGuestBytes`, `WriteGuestBytes`, `Devices()`, `SetLastError`, `LastError`)를 기본 구현과 함께 추가했다.
  *Added optional services to **`ImportCallServices`** (`ReadGuestBytes`, `WriteGuestBytes`, `Devices()`, `SetLastError`, `LastError`) with default implementations.*
- **`kernel32` facade.** `CreateFileA`가 장치를 연다. 실패하면 `\\.\` 이름은 123, 그 밖의 이름은 2를 last error로 둔다. `DeviceIoControl`(인자 8개, in-place buffer 공유, `lpBytesReturned`), `CloseHandle`, `GetLastError`, `SetLastError`를 추가해 export는 9개가 됐다.
  ***`kernel32` facade.** `CreateFileA` opens devices, with last error 123 for failed `\\.\` names and 2 otherwise; added `DeviceIoControl` (eight arguments, shared in-place buffer, `lpBytesReturned`), `CloseHandle`, `GetLastError`, and `SetLastError`, for nine exports.*
- **Linux 연결.** `NativeKernel32Diagnostic`이 장치 set과 last error를 소유하고 서비스로 제공한다. guest byte 접근은 image 또는 guest stack 범위로 제한한다. `RunOriginalInProcessContinuation`이 `GuestDeviceConfig`를 받는다. CLI(`BuildLinuxGuestDevices`)는 프로파일과 작업 360의 `ResolveHardlockDeviceMaterial`로 설정을 만들고, 재료 적용 여부와 요청 종류별 횟수만 출력한다. 인자 기록 상한은 8이 됐다.
  ***Linux wiring.** `NativeKernel32Diagnostic` owns the device set and last error and provides them as services, limiting guest byte access to image or guest-stack ranges; `RunOriginalInProcessContinuation` takes a `GuestDeviceConfig`; the CLI (`BuildLinuxGuestDevices`) builds it from the profile and Task 360's `ResolveHardlockDeviceMaterial`, printing only whether material was applied and the counts per kind. The argument record limit is now eight.*
- **단위 테스트.** 메모리 기반 서비스로 장치 export를 검사한다. 장치·비장치 `CreateFileA`와 last error, 0 byte initialize, in-place handshake, 크기 거절(13), 처리하지 않는 code(1), 모르는 handle(6), 두 번 닫기, last error 왕복을 확인한다.
  ***Unit tests.** Memory-backed services exercise the device exports: device and non-device `CreateFileA` with last error, a zero-byte initialize, an in-place handshake, a rejected size (13), an unhandled code (1), an unknown handle (6), closing twice, and a last-error round trip.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 통과 / no warnings or errors, 3/3 each |
| Linux helper 스크립트, in-process probe, 기존 진단 네 개 / helper script, probes, four diagnostics | 통과, 작업 360과 같음(trace 43 frame 포함) / pass, same as Task 360, including the 43-frame trace |
| Windows x86 build, CTest | 오류·경고 없음. 6개 중 5개 통과(기존 `re2dj_windows_vfs_runtime_probe` 실패만 남음) / no errors or warnings; 5 of 6, with only the known probe failure |

실제 4th CHD(`roms/ez2dj4th/4thTrax.chd`, 읽기 전용)의 기본 `re2dj ez2dj4th` 실행은 x64와 x86이 같다. 사용자 `cfg/hardlock.ini`를 적용했고(`hardlock material: applied`), 값은 출력하지 않았다.

*The default `re2dj ez2dj4th` run on the real 4th CHD (`roms/ez2dj4th/4thTrax.chd`, read-only) is identical on x64 and x86, with the user's `cfg/hardlock.ini` applied (`hardlock material: applied`) and no values printed:*

```text
#0014 CreateFileA "\\.\FEnteDev" -> 00001004
#0015 GetProcAddress(kernel32, "CloseHandle") -> 6f002072
#0016 GetProcAddress(kernel32, "DeviceIoControl") -> 6f00205f
#0017 DeviceIoControl(00001004, 9c402468, 0, 0, 0, 0, <stack>, 0) -> 1
#0018 CloseHandle(00001004) -> 1
#0019 GetProcAddress(user32, "GetActiveWindow") -> 6eff2000
#0020 GetProcAddress(user32, "CreateCursor") -> 0
hardlock requests: total=1 initialize=1 ... last=initialize/completed
continuation    : stopped at unresolved lookup GetProcAddress(6eff0000, CreateCursor)
```

해석은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)의 작업 361 절에 두었다.

*The interpretation is in the Task 361 section of the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md).*

## 다음 / Next

작업 360에서 세운 계획은 다음 단계를 `wtsapi32` facade로 잡았다. 그러나 게스트가 initialize 뒤 실제로 먼저 요구한 것은 `user32!CreateCursor`다. 다음 작업은 initialize 뒤 게스트가 요구하는 export를 차례로 제공하고, WTS 세션 조회에 도달하면 `wtsapi32` facade(세션 번호 0)를 연결하는 순서로 한다.

*Task 360's plan put the `wtsapi32` facade next, but after the initialize the guest actually asks first for `user32!CreateCursor`. The next work provides the exports the guest requests after the initialize in order, connecting the `wtsapi32` facade (session ID 0) when the WTS session query is reached.*
