# 작업 361 설계 — 게스트 장치 handle과 `kernel32` 장치 호출 / Task 361 design — Guest device handles and `kernel32` device calls

선행: [작업 360 설계](20260924-360-hardlock-hle-shared-boundary.md), [작업 359 작업 로그](../work-logs/20260924-359-user32-message-box.md), [작업 340 설계](20260921-340-guest-pe-compatibility-modules.md) 5단계

## 배경 / Background

Linux in-process 실행에서는 `kernel32` facade의 `CreateFileA`가 모든 이름에 `INVALID_HANDLE_VALUE`를 돌려준다. 그래서 4th가 `\\.\FEnteDev`를 열지 못하고 Hardlock Error 1009로 끝난다. Windows에서는 같은 이름이 가짜 장치로 열리고, 그 뒤 `DeviceIoControl` 요청을 Hardlock 장치가 처리한다. 작업 360에서 장치 재료 조립, Win32 완료 규칙, 장치 경로 판정을 공용으로 옮겼으므로, 이제 Linux facade에 연결할 수 있다.

*In Linux in-process runs the `kernel32` facade's `CreateFileA` returns `INVALID_HANDLE_VALUE` for every name, so 4th cannot open `\\.\FEnteDev` and ends in Hardlock Error 1009. On Windows the same name opens a synthetic device whose `DeviceIoControl` requests the Hardlock device answers. With Task 360 sharing the material assembly, the Win32 completion rules, and the device-path match, they can now be connected to the Linux facade.*

## 결정 / Decision

### 1. 공용 `GuestDeviceSet` / Shared `GuestDeviceSet`

`include/re2dj/hle/guest_devices.h`에 플랫폼 중립 `GuestDeviceSet`을 둔다. 게스트가 여는 장치와 그 handle을 소유한다.

*A platform-neutral `GuestDeviceSet` in `include/re2dj/hle/guest_devices.h` owns the devices the guest opens and their handles.*

- `Open(name)`: 이름이 프로파일의 장치 prefix와 맞으면(`MatchesGuestDevicePrefix`) 새 handle을 돌려주고, 아니면 0을 돌려준다. handle 값은 Win32 kernel handle처럼 4의 배수이며 `0x00001004`부터 시작한다. 0과 `INVALID_HANDLE_VALUE`는 쓰지 않는다.
  *`Open(name)` returns a new handle when the name matches the profile's device prefix (`MatchesGuestDevicePrefix`) and 0 otherwise; handles are multiples of four like Win32 kernel handles, starting at `0x00001004`, never 0 or `INVALID_HANDLE_VALUE`.*
- `Control(handle, code, input, output)`: 열린 장치 handle이면 작업 360의 `CompleteHardlockDeviceIoControl`로 처리하고 요청 통계를 기록한다.
  *`Control(handle, code, input, output)` answers an open device handle through Task 360's `CompleteHardlockDeviceIoControl` and records the request counts.*
- `Close(handle)`.
- Hardlock 장치 옵션은 한 번 만들어 모든 요청에 쓴다. Windows runtime은 요청마다 전역 변수에서 옵션을 다시 만들지만, 옵션은 실행 중 바뀌지 않으므로 결과는 같다.
  *The Hardlock device options are built once and reused; the Windows runtime rebuilds them from globals per request, but they never change during a run, so the result is the same.*

### 2. handler 서비스 확장 / Handler services

`ImportCallServices`에 다음을 추가한다. 기본 구현은 "지원 안 함"이라서, 기존 구현체와 IPC 경로는 바뀌지 않는다.

*Add the following to `ImportCallServices`, with "unsupported" default implementations so existing implementers and the IPC path are unchanged:*

- `ReadGuestBytes`/`WriteGuestBytes`: `DeviceIoControl`의 입출력 buffer와 `lpBytesReturned`를 다루는 데 쓴다. Linux 구현은 문자열 읽기와 같은 정책을 따른다. 즉 image 또는 guest stack 범위만 허용한다. 아직 heap facade가 없어서 게스트 buffer는 이 두 곳에만 있을 수 있다.
  *`ReadGuestBytes`/`WriteGuestBytes` for `DeviceIoControl` buffers and `lpBytesReturned`; the Linux implementation follows the string-read policy of image or guest-stack ranges only, the only places guest buffers can live until a heap facade exists.*
- `Devices()`: 이 실행의 `GuestDeviceSet`. 없으면 null이다.
  *`Devices()`: the run's `GuestDeviceSet`, or null.*
- `SetLastError`/`LastError`: 스레드 last-error 값. 이번 단계에서는 host 쪽에 보관한다. 게스트가 TEB `+0x34`를 직접 읽는 경우는 아직 반영하지 않는다(단일 thread).
  *`SetLastError`/`LastError`: the thread's last-error value, kept host-side for now; a guest reading TEB `+0x34` directly is not yet reflected (single thread).*

### 3. `kernel32` facade / `kernel32` facade

| export | 인자 / Args | 동작 / Behavior |
| --- | --- | --- |
| `CreateFileA` | 7 | 이름을 읽어 `Devices()->Open`에 넘긴다. 성공하면 handle을 돌려주고 last error는 0이다. 실패하면 `INVALID_HANDLE_VALUE`를 돌려준다. last error는 `\\.\`로 시작하는 이름이면 123(`ERROR_INVALID_NAME`, Windows host에서 4th의 `\\.\NTICE` 열기가 받은 값)이고, 그 밖의 이름은 2(`ERROR_FILE_NOT_FOUND`, VFS가 아직 없음)다. / Opens through `Devices()`; failure returns `INVALID_HANDLE_VALUE` with last error 123 for `\\.\` names (the value 4th's `\\.\NTICE` open received on the Windows host) or 2 for other names (no VFS yet) |
| `DeviceIoControl` | 8 | 입력 buffer와 출력 buffer를 guest memory에서 읽어 `Devices()->Control`에 넘긴다. 두 buffer가 같은 주소·크기면 같은 host buffer를 공유시켜, Windows에서 in-place로 처리되는 것과 같게 한다. 출력과 `*lpBytesReturned`를 다시 쓰고, `BOOL`과 last error를 설정한다. 모르는 handle은 `ERROR_INVALID_HANDLE`(6)이고, 장치가 처리하지 않는 code는 `ERROR_INVALID_FUNCTION`(1)이다. `lpOverlapped`는 지원하지 않는다. / Reads the buffers, sharing one host buffer when both have the same address and size to match Windows' in-place handling; writes back the output and `*lpBytesReturned`; unknown handle → 6, unhandled code → 1; no `lpOverlapped` |
| `CloseHandle` | 1 | 장치 handle을 닫는다. 모르는 handle은 `FALSE`, 6이다. / Closes a device handle; unknown → `FALSE`, 6 |
| `GetLastError`, `SetLastError` | 0, 1 | 서비스의 last error / the services' last error |

([CreateFileA](https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-createfilea), [DeviceIoControl](https://learn.microsoft.com/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol), [CloseHandle](https://learn.microsoft.com/windows/win32/api/handleapi/nf-handleapi-closehandle), [GetLastError](https://learn.microsoft.com/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror), [System Error Codes](https://learn.microsoft.com/windows/win32/debug/system-error-codes--0-499-))

### 4. Linux 연결 / Linux wiring

CLI가 대상 프로파일에서 장치 설정을 만든다. 프로파일이 장치를 허용하면(`device_mock_enabled`) prefix를 넘기고, `hardlock_cfg_material_default`이면 작업 360의 `ResolveHardlockDeviceMaterial`로 사용자 `cfg/hardlock.ini`와 map을 읽는다. 이 설정을 `RunOriginalInProcessContinuation`에 넘기면 `NativeKernel32Diagnostic`이 `GuestDeviceSet`을 소유하고 서비스로 제공한다. 설정 값과 seed는 어떤 로그에도 남기지 않는다.

*The CLI builds the device configuration from the target profile: the prefix when the profile allows the device (`device_mock_enabled`), and Task 360's `ResolveHardlockDeviceMaterial` over the user's `cfg/hardlock.ini` and map when `hardlock_cfg_material_default` is set. `RunOriginalInProcessContinuation` receives it, and `NativeKernel32Diagnostic` owns the `GuestDeviceSet` and exposes it through the services. No material value or seed is logged.*

continuation 기록은 인자를 최대 8개까지 남긴다(`DeviceIoControl`). CLI는 실행 끝에 Hardlock 요청 통계를 Windows와 같은 종류별 횟수로 출력한다.

*The continuation record keeps up to eight arguments (`DeviceIoControl`), and the CLI prints the Hardlock request counts per kind at the end of the run, as on Windows.*

## 검증 / Validation

- 단위 테스트: `GuestDeviceSet`의 prefix 열기, handle 값, 닫기, 모르는 handle을 검사한다. `kernel32`의 `CreateFileA`(장치/비장치 last error), `DeviceIoControl`(in-place, 크기 거절, 모르는 handle, `lpBytesReturned`), `CloseHandle`, last error도 검사한다. 이를 위해 메모리 기반 테스트용 서비스를 쓴다.
  *Unit tests: `GuestDeviceSet` prefix opens, handle values, closing, and unknown handles; `kernel32` `CreateFileA` (device and non-device last error), `DeviceIoControl` (in-place, rejected size, unknown handle, `lpBytesReturned`), `CloseHandle`, and last error, through memory-backed test services.*
- 실제 4th(Linux 두 폭): `\\.\FEnteDev`가 열리고 `0x9c402468` initialize가 처리되는지 확인한다. 다음 경계를 기록하고, Windows 기준(initialize 뒤 WTS 세션 조회, handshake `0x450`)과 비교한다.
  *Real 4th (Linux, both widths): `\\.\FEnteDev` opens and the `0x9c402468` initialize is answered; record the next boundary and compare with Windows (a WTS session query after initialize, then handshake `0x450`).*
- Windows build와 CTest(공용 코어·서비스 interface 변경 회귀).
  *Windows build and CTest (regression for the shared core and services interface).*
