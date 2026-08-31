# ez2dj3rd Hardlock status 경계 진단 작업 로그

관련 설계: [ez2dj3rd Hardlock status 경계 진단](../design/20260831-108-ez2dj3rd-hardlock-status-oracle.md)

*Related design: [ez2dj3rd Hardlock status-boundary diagnostic](../design/20260831-108-ez2dj3rd-hardlock-status-oracle.md).*

## 결과

- 동적 `GetProcAddress` 진단으로 원본이 `WTSQuerySessionInformationA`와 `WTSFreeMemory`를 resolve함을 확인했습니다.
- 전달형 wrapper는 session ID, info class, 성공 여부, 반환 크기와 최대 4바이트 scalar만 bounded 로그에 기록하고 반환값, allocation, `LastError`를 보존합니다.
- 기본 경로는 host 결과를 바꾸지 않습니다. 명시적 `--device-mock-wts-console-session`에서만 `WTS_CURRENT_SESSION`, class 4, 성공한 정확히 4바이트 결과를 active 상태 `0`으로 바꿉니다.
- zero-byte-success 기준선 `20260831-235043-995`는 host 상태 `3`을 세 번 관찰하고 `0x9c402468`만 기록했습니다.
- 동일 zero-byte-success에 active-session HLE를 적용한 `20260831-234743-174`는 상태 `0`과 `0x9c402450`을 각각 세 번 기록했습니다.
- full-size-success 대조 `20260831-234941-553`도 상태 `0`과 `0x9c402450`을 각각 세 번 기록했습니다. 따라서 `0x450` 도달은 IOCTL 반환 길이가 아니라 WTS session 상태 HLE에 의해 발생합니다.
- 세 active-session 관찰은 `0x9c40244c/458`에 도달하지 않았습니다. zero-byte와 full-size 모두 output buffer를 보존했으므로 다음 경계에는 유효한 6바이트 driver-written payload가 필요합니다.
- 이번 최종 비교에서는 guest descriptor status memory를 직접 변경하지 않았습니다. status `38` 자체의 독립 인과성은 미확정으로 유지합니다.
- 각 bounded 실행에서 실제 경로를 확인한 해당 child PID `7804`, `20780`, `15060`만 종료했고 launcher는 모두 정상 종료했습니다. 원본 HDD와 overlay는 변경하지 않았습니다.

*Dynamic `GetProcAddress` diagnostics confirmed that the original resolves `WTSQuerySessionInformationA` and `WTSFreeMemory`. A forwarding wrapper records only bounded session/class/result/size/scalar data while preserving the return value, allocation, and `LastError`. The default path does not alter the host result; only explicit `--device-mock-wts-console-session` changes a successful, exactly four-byte `WTS_CURRENT_SESSION` class-4 result to active state `0`. Zero-byte-success baseline `20260831-235043-995` observed host state `3` three times and only `0x9c402468`. With the active-session HLE, zero-byte run `20260831-234743-174` recorded state `0` and `0x9c402450` three times each. Full-size comparison `20260831-234941-553` produced the same result. Reaching `0x450` is therefore caused by the WTS session-state HLE, not the IOCTL byte count. None reached `0x44c/458`; a valid driver-written six-byte payload remains necessary. The final comparison did not directly change guest descriptor status memory, so status `38` remains independently unresolved. Only the path-verified child PIDs `7804`, `20780`, and `15060` were terminated; every launcher exited normally, and neither the original HDD nor overlay was modified.*

## 구현

- Windows injected runtime에 bounded dynamic-resolver route 진단과 WTS 전달 wrapper를 추가했습니다.
- launcher에 기본 비활성 분석 옵션과 runtime export 주입을 연결했습니다.
- 전용 Hardlock descriptor probe가 일반 Win32 forwarding, HLE route, WTS active override와 trace marker를 함께 검증합니다.

*The Windows injected runtime now has bounded dynamic-resolver route diagnostics and a forwarding WTS wrapper. The launcher injects a default-off analysis policy through a runtime export. The dedicated Hardlock descriptor probe jointly verifies ordinary Win32 forwarding, the HLE route, the WTS active override, and its trace marker.*

## 검증

- Windows x86 Debug launcher 및 Hardlock descriptor probe 빌드
- Windows x86 Debug CTest 4/4 통과: VFS runtime, Hardlock descriptor, product loader, unit tests
- 원본 bounded 3-way 비교: host session baseline / zero-byte active HLE / full-size active HLE
- `git diff --check`

*Verification covers the Windows x86 Debug launcher and Hardlock descriptor-probe build, all four Windows x86 Debug CTest targets, the bounded three-way original comparison, and `git diff --check`.*

## 다음 경계

`0x9c402450`의 6바이트 request/response 소비와 `FA FA` marker 검사를 복원합니다. 실제 driver-written payload가 확인되기 전에는 zero/full-size buffer-preserving 결과를 유효 Hardlock 응답으로 간주하지 않으며, Task 107 seed 후보도 실제 seed로 승격하지 않습니다.

*Next, reconstruct the six-byte `0x9c402450` request/response consumption and `FA FA` marker check. Until a real driver-written payload is established, neither buffer-preserving zero/full-size results nor any Task 107 seed candidate is promoted to a valid Hardlock response or physical seed.*
