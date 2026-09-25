# 작업 361 작업 지시서 — 게스트 장치 handle과 `kernel32` 장치 호출 / Task 361 work order — Guest device handles and `kernel32` device calls

설계: [20260924-361-linux-guest-device-handles.md](../design/20260924-361-linux-guest-device-handles.md)

## 단계 / Steps

1. `hle/guest_devices`(`GuestDeviceSet`)를 추가한다. `ImportCallServices`에 guest byte 읽기·쓰기, `Devices()`, last error를 기본 구현과 함께 추가한다.
2. `kernel32` facade의 `CreateFileA`를 장치 열기로 바꾸고, `DeviceIoControl`·`CloseHandle`·`GetLastError`·`SetLastError`를 추가한다. 단위 테스트를 쓴다.
3. `NativeKernel32Diagnostic`에 장치 set과 서비스를 구현한다. `RunOriginalInProcessContinuation`에 장치 설정을 넘기고, CLI가 프로파일과 `cfg/hardlock.ini`에서 설정을 만든다. 인자 기록 상한을 8로 올리고, Hardlock 통계를 출력한다.
4. Linux x64·x86, Windows x86을 빌드하고 CTest를 실행한다. 실제 4th를 Linux 두 폭에서 실행해 새 경계를 확인한다.
5. 분석, `ARCHITECTURE.md`, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) add `hle/guest_devices` (`GuestDeviceSet`) and extend `ImportCallServices` with guest byte reads/writes, `Devices()`, and last error, all with default implementations; (2) turn the `kernel32` facade's `CreateFileA` into a device open and add `DeviceIoControl`, `CloseHandle`, `GetLastError`, and `SetLastError`, with unit tests; (3) implement the device set and services in `NativeKernel32Diagnostic`, pass the device configuration to `RunOriginalInProcessContinuation`, have the CLI build it from the profile and `cfg/hardlock.ini`, raise the argument record limit to eight, and print the Hardlock counts; (4) build Linux x64/x86 and Windows x86, run CTest, and run the real 4th on both Linux widths to observe the new boundary; (5) update the analysis, `ARCHITECTURE.md`, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* 단위 테스트가 통과하고 Windows·Linux build 회귀가 없다.
* 실제 4th가 Linux 두 폭에서 `\\.\FEnteDev`를 열고 `0x9c402468`이 처리되며, 다음 경계가 두 폭에서 같게 기록된다.

*Completion: the unit tests pass without Windows or Linux build regressions, and on both Linux widths the real 4th opens `\\.\FEnteDev`, its `0x9c402468` is answered, and the next boundary is recorded identically.*
