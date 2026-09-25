# 작업 360 작업 로그 — Hardlock HLE 연결부 공용화와 WTS 이름 정정 / Task 360 work log — Sharing the Hardlock HLE wiring and correcting the WTS names

설계: [20260924-360-hardlock-hle-shared-boundary.md](../design/20260924-360-hardlock-hle-shared-boundary.md)
작업 지시서: [20260924-360-hardlock-hle-shared-boundary.md](../work-orders/20260924-360-hardlock-hle-shared-boundary.md)

## 변경 / Changes

- **A. `hle/hardlock/device_material`.** `ResolveHardlockDeviceMaterial`와 `MakeHardlockDeviceOptions`를 추가했다. launcher의 cfg 절 읽기, 기본 map 선택, seed 해석, replay 값 채우기, hex·map 해석을 옮겼다. launcher는 이 함수를 부르고, 주입 runtime 배열의 용량 검사와 원격 전달만 직접 한다. 이전에 hex 문자열이 비어 있는지로 판정하던 "사용함" 표시(`handshake`·`tail`)는 Boolean으로 명시했다.
  ***A. `hle/hardlock/device_material`.** Added `ResolveHardlockDeviceMaterial` and `MakeHardlockDeviceOptions`, moving the launcher's cfg section read, default-map choice, seed parsing, replay filling, and hex/map parsing; the launcher calls it and keeps only the injected-runtime capacity check and cross-process transport. The "in use" markers for the handshake and tail, formerly inferred from non-empty hex strings, are now explicit Booleans.*
- **B. `hle/hardlock/device_call`.** `CompleteHardlockDeviceIoControl`, `HardlockDeviceActivity`와 `RecordHardlockDeviceCall`, `FormatHardlockDeviceTrace`를 추가했다. 주입 runtime의 `CompleteHardlockRequest`는 이것을 쓰고, 통계 구조체는 공용 타입의 별칭이 됐다. trace 줄 형식은 byte 단위로 같다.
  ***B. `hle/hardlock/device_call`.** Added `CompleteHardlockDeviceIoControl`, `HardlockDeviceActivity` with `RecordHardlockDeviceCall`, and `FormatHardlockDeviceTrace`; the injected runtime's `CompleteHardlockRequest` uses them, and its activity struct is now an alias of the shared type. The trace line format is byte-identical.*
- **C. `hle/guest_device_path`.** `MatchesGuestDevicePrefix`를 추가하고 주입 runtime의 `HasDeviceMockPrefix`가 이를 쓴다. 빈 prefix일 때 `\\.\lptdi`를 쓰는 기본값은 runtime에 남겼다.
  ***C. `hle/guest_device_path`.** Added `MatchesGuestDevicePrefix`, used by the injected runtime's `HasDeviceMockPrefix`; the `\\.\lptdi` fallback for an empty prefix stays in the runtime.*
- **D. WTS 이름 정정.** 프로파일 필드 `hle_wts_active_console` → `hle_wts_console_session`, runtime 상수 `kWtsConnectState` → `kWtsSessionId`로 바꿨다. 관련 주석, 오류 문구("console session without a device policy"), product loader probe의 기대 문구도 함께 고쳤다. `ARCHITECTURE.md`와 4th·3rd·1st SE 분석 문서에 정정을 반영했다. 작업 로그는 시간순 증거라 고치지 않았다.
  ***D. WTS name correction.** Renamed the profile field `hle_wts_active_console` → `hle_wts_console_session` and the runtime constant `kWtsConnectState` → `kWtsSessionId`, with the related comments, the error text ("console session without a device policy"), and the product loader probe's expected text; reflected the correction in `ARCHITECTURE.md` and the 4th, 3rd, and 1st SE analyses, leaving work logs as chronological evidence.*
- **단위 테스트.** `hardlock_device_material_test.cpp`를 추가했다. 합성 값만 쓴다. A는 파일 없음·절 없음, cfg 비허용, seed와 replay, 명시 옵션 우선, 기본 map, 해석 불가 seed, 잘못된 hex와 없는 map을 검사한다. B는 미처리·완료·형태 거절의 Win32 결과, 통계, trace 형식을 검사한다. C는 대소문자, 가변 꼬리, 빈 prefix를 검사한다.
  ***Unit tests.** Added `hardlock_device_material_test.cpp` with synthetic values only: A covers no file or section, cfg disallowed, seeds and replay, explicit precedence, the default map, unparseable seeds, bad hex, and a missing map; B covers the Win32 result for unhandled, completed, and rejected-shape requests, the counts, and the trace format; C covers case, a varying tail, and an empty prefix.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build | 오류·경고 없음 / no errors or warnings |
| Windows x86 CTest | 6개 중 5개 통과(`re2dj_unit_tests`, `re2dj_windows_product_loader_probe` 포함). `re2dj_windows_vfs_runtime_probe`는 작업 358에서 기존 문제로 확인한 같은 메시지로 실패 / 5 of 6, including unit tests and the product loader probe; the known `re2dj_windows_vfs_runtime_probe` failure with the same message |
| Linux x64·x86 build, CTest | 각각 3/3 통과. x64 실제 4th continuation은 `ExitProcess(9)`로 이전과 같음 / 3/3 each; the x64 real-4th continuation still ends in `ExitProcess(9)` |

**Windows 실제 4th 회귀.** 사용자 `cfg/hardlock.ini`로 제품 명령 `re2dj ez2dj4th`를 변경 전 build(`20260924-165054-937`)와 변경 후 build(`20260924-170049-184`)로 각각 30초 실행했다. 두 실행의 Hardlock 기록이 같다.

***Real 4th regression on Windows.** The product command `re2dj ez2dj4th` ran for 30 seconds each with the user's `cfg/hardlock.ini` on the pre-change build (`20260924-165054-937`) and the post-change build (`20260924-170049-184`), with identical Hardlock records:*

| 요청 / Request (tick 제외 / tick excluded) | 변경 전 / before | 변경 후 / after |
| --- | --- | --- |
| `initialize` completed, 0 byte | 1 | 1 |
| `handshake` completed, 6 byte, `handshake_answered=1` | 4 | 4 |
| `transform` completed, 264 byte, `mapped=1` | 36 | 36 |
| `descriptor` completed, 256 byte, `tail=0` | 42 | 42 |
| `descriptor` completed, 256 byte, `tail=1` | 2 | 2 |
| `hardlock_cfg_material` | `response450=true, tail44c=true, map=false` | 같음 / same |
| `hardlock_seeds`, `hardlock_device` 기록 / records | 있음 / present | 같음(값은 hash로만 비교) / same (compared by hash only) |

seed 값과 module address는 로그와 이 문서 어디에도 출력하지 않았다.

*Seed values and the module address were never printed to the terminal or this document.*

## 다음 / Next

작업 361: 게스트 handle 표와 `kernel32`의 `CreateFileA`(장치)·`DeviceIoControl`·`CloseHandle`·last error를 Linux facade에 연결한다. 이번에 공용으로 옮긴 A·B·C를 그대로 쓴다.

*Task 361: connect guest handles and `kernel32` `CreateFileA` (devices), `DeviceIoControl`, `CloseHandle`, and last error in the Linux facade, reusing the A, B, and C shared here.*
