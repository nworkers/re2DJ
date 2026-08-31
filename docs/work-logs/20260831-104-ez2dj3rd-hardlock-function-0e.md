# ez2dj3rd Hardlock Function 0x0e 분석 작업 로그

## 결과

3rd 프로파일이 원본 보호 코드의 동적 진입 경계까지 도달하는 것은 확인했습니다. 다만 실제 Hardlock 인증에 필요한 `Function 0x0e` 8바이트 응답이나 이를 생성하는 seed를 확인하지 못했으므로, 이번 작업에서 Hardlock 통과를 완료했다고 판정하지 않습니다.

The 3rd profile was confirmed to reach the dynamic boundary used by the original protection code. The actual Hardlock pass is not considered complete because the valid eight-byte `Function 0x0e` response, or the seeds that generate it, was not recovered.

## 확인된 실행 흐름

- 3rd 실행 파일에는 `KERNEL32.dll!GetProcAddress` IAT 슬롯이 두 개 있으며, 두 슬롯을 모두 런타임 resolver로 연결해야 보호 경로가 진행됩니다.
- 동적 장치 조회에서 `\\.\NTICE`와 `\\.\FEnteDev`를 확인했습니다. `FEnteDev`를 3rd 프로파일의 synthetic device path로 설정했습니다.
- synthetic `FEnteDev` handle에 대한 계측에서 `0x9c402468`, `0x9c402450`, `0x9c40244c`, `0x9c402458` 요청을 확인했습니다.
- `0x9c40244c`는 API version `0x4703`와 `Function 0`, 이후 `Function 6`을 담는 256바이트 descriptor 경계입니다.
- `0x9c402450`은 6바이트 버퍼와 `FA FA` marker를 사용하는 하위 장치 응답 경계입니다.
- `0x9c402458`은 256바이트 descriptor와 뒤의 가변 8바이트 암호 블록을 포함한 264바이트 입출력 경계이며, descriptor의 Function은 `0x0e`입니다.

## 구현 변경

- 동일한 import 이름의 모든 IAT 슬롯을 검색할 수 있도록 PE verifier helper를 확장했습니다.
- Windows x86 launcher가 3rd의 두 `GetProcAddress` 슬롯을 모두 injected runtime resolver에 연결하도록 변경했습니다.
- 3rd 기본 synthetic device path를 원본에서 관찰된 `\\.\FEnteDev`로 변경했습니다.
- `NTICE` 조회는 별도 역할일 가능성이 있어 자동 성공시키지 않았습니다.
- 현재의 zero target state나 1st SE의 LPTDI 응답 변환을 3rd Hardlock seed 또는 0x0e 응답으로 취급하지 않았습니다.

## 검증

- Windows x86 Debug 대상 빌드 성공.
- CTest `3/3` 통과.
- `re2dj ez2dj3rd` bounded 실행에서 `device_mock_dynamic_resolver_slots=2`와 `FEnteDev` device-open 성공을 확인했습니다.
- 직접 launcher 실행은 runtime detach까지 도달했지만, 이는 Hardlock 인증 성공을 의미하지 않습니다. Product 실행은 bounded 검증 제한으로 종료했으며 게임 화면 도달은 확인하지 못했습니다.

## 미완료 및 다음 입력

1. 3rd Hardlock의 유효한 `Function 0x0e` 입출력 쌍을 확보합니다.
2. 또는 합법적으로 보유한 3rd dongle dump에서 세 개의 16비트 seed를 추출합니다.
3. 입력을 확보하면 프로파일별 seed/응답 정책을 별도 상태로 구현하고 원본 후속 실행 또는 게임 화면 도달로 재검증합니다.

원본 HDD 자산, 실행 파일, dump는 저장소에 추가하지 않습니다.

## 관련 문서

- `docs/design/20260831-104-ez2dj3rd-hardlock-function-0e.md`
- `docs/analysis/ez2dj3rd-hardlock-function-0e.md`
- `docs/analysis/ez2dj-exe-structures.md`
- `docs/design/20260830-103-hardlock-dynamic-import-hook.md`

---

# Work Log: ez2dj3rd Hardlock Function 0x0e Analysis

## Result

The 3rd profile was confirmed to reach the dynamic boundary used by the original protection code. The actual Hardlock pass is not considered complete because the valid eight-byte `Function 0x0e` response, or the seeds that generate it, was not recovered.

## Confirmed execution flow

- The 3rd executable contains two `KERNEL32.dll!GetProcAddress` IAT slots, and both must be routed to the injected runtime resolver for the protected path to continue.
- Dynamic device queries requested `\\.\NTICE` and `\\.\FEnteDev`. The latter is now the 3rd profile's synthetic device path.
- Instrumentation on a synthetic `FEnteDev` handle observed requests for `0x9c402468`, `0x9c402450`, `0x9c40244c`, and `0x9c402458`.
- `0x9c40244c` is a 256-byte descriptor boundary with API version `0x4703`, initial `Function 0`, and later `Function 6`.
- `0x9c402450` is a lower-level device-response boundary using a six-byte buffer and an `FA FA` marker.
- `0x9c402458` is a 264-byte input/output boundary containing the 256-byte descriptor followed by a changing eight-byte encrypted block; the descriptor function is `0x0e`.

## Implementation changes

- Extended the PE verifier helper to find every IAT slot for the same import name.
- Updated the Windows x86 launcher to route both 3rd `GetProcAddress` slots to the injected runtime resolver.
- Changed the 3rd default synthetic device path to the observed `\\.\FEnteDev`.
- Did not automatically succeed the separate `NTICE` query because its role remains unresolved.
- Did not treat the current zero target state or the 1st SE LPTDI transform as a 3rd Hardlock seed or 0x0e response.

## Verification

- Windows x86 Debug targets built successfully.
- CTest passed `3/3` tests.
- A bounded `re2dj ez2dj3rd` run confirmed `device_mock_dynamic_resolver_slots=2` and a successful `FEnteDev` device open.
- The direct launcher reached runtime detach, but this does not prove Hardlock authentication. The product run was stopped by the bounded verification limit, and no game screen was observed.

## Incomplete work and required next input

1. Obtain a valid 3rd Hardlock `Function 0x0e` input/output pair.
2. Alternatively, extract the three 16-bit seeds from a legally owned 3rd dongle dump.
3. Once available, implement profile-specific seed/response state and re-verify by observing normal original-code continuation or the game screen.

Original HDD assets, executables, and dumps must not be added to the repository.
