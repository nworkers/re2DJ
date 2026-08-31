# ez2dj3rd Hardlock 0x9c402450 응답 경계 작업 로그

관련 설계: [ez2dj3rd Hardlock 0x9c402450 응답 경계](../design/20260831-109-ez2dj3rd-hardlock-450-response.md)

*Related design: [ez2dj3rd Hardlock `0x9c402450` response boundary](../design/20260831-109-ez2dj3rd-hardlock-450-response.md).*

## 결과

- zero-byte 실행 `20260901-000336-290`과 full-size 실행 `20260901-000528-846`에서 현재 6바이트 in-place request `01 00 00 00 03 00`이 각각 세 번 동일하게 관찰됐습니다.
- `20260901-000702-177`의 `0x450` 전용 256-step trace는 marker word가 `0xFAFA`와 다르면 helper가 0을 반환하고, 같으면 세 번째 word를 반환함을 확인했습니다.
- 현재 marker 0은 상위 unsigned `0x033c` 비교의 낮은 경로로 들어가 descriptor status 11을 기록합니다.
- 과거 synthetic oracle `01 00 FA FA 00 10`을 명시적으로 replay한 `20260901-001743-276`은 `0x9c40244c` Function 0 descriptor에 두 번 도달했습니다.
- `0x458`은 관찰되지 않았습니다. replay bytes는 실제 dongle/driver 응답이 아니며 `0x44c` 도달 인과성만 확인합니다.
- 네 실행의 해당 child PID `33012`, `26984`, `36148`, `34416`만 경로 확인 뒤 종료했습니다. 원본 HDD와 overlay는 변경하지 않았습니다.

*Zero-byte run `20260901-000336-290` and full-size run `20260901-000528-846` each recorded the same current six-byte in-place request `01 00 00 00 03 00` three times. The `0x450`-filtered 256-step trace in `20260901-000702-177` confirmed that marker mismatch returns zero, while marker `0xFAFA` returns the third word; the current zero marker selects the below-`0x033c` path and writes descriptor status 11. Explicit replay of historical synthetic oracle `01 00 FA FA 00 10` in `20260901-001743-276` reached a Function-0 `0x9c40244c` descriptor twice. It did not reach `0x458`; the replay bytes are not a physical dongle/driver response and establish only causality for `0x44c` reachability. Only path-verified child PIDs `33012`, `26984`, `36148`, and `34416` were terminated, and neither original HDD nor overlay was changed.*

## 구현

- exact `0x450`, exact 6-byte 호출만 기록하는 최대 16회 bounded marker를 추가했습니다.
- 플랫폼 중립 `hardlock_450_response` parser는 정확히 12자리 hex만 허용합니다.
- launcher의 `--device-mock-hardlock-450-response`는 기본 비활성이며 지정된 6바이트를 runtime export로 전달합니다.
- runtime replay는 synthetic device의 exact `0x450`에만 output을 복사하고 6 bytes returned와 success를 반환합니다. 다른 control code와 기본 정책은 유지합니다.
- 전용 runtime probe와 단위 테스트가 packet marker, replay output과 parser 오류를 검증합니다.

*Implementation adds a sixteen-entry bounded marker for exact six-byte `0x450`, a platform-neutral parser accepting exactly twelve hex digits, and default-off launcher option `--device-mock-hardlock-450-response`. Runtime replay copies the supplied output and returns six-byte success only for an exact synthetic-device `0x450`; other control codes and default policies remain unchanged. Dedicated runtime and unit tests cover the marker, replay output, and parser errors.*

## 검증

- Windows x86 Debug launcher, injected runtime, Hardlock descriptor probe와 unit tests 빌드
- 전용 Hardlock descriptor probe 통과
- unit tests `1005` checks, `0` failures
- active-session bounded 원본 비교 4회
- Windows x86 Debug CTest 4/4
- `git diff --check`

*Verification includes the Windows x86 Debug launcher, injected runtime, dedicated Hardlock probe and unit-test build; a passing dedicated probe; 1,005 unit checks with zero failures; four bounded active-session original comparisons; Windows x86 Debug CTest 4/4; and `git diff --check`.*

## 다음 경계

다음 작업은 `0x9c40244c` Function 0 descriptor의 in-place 응답과 status 소비를 추적합니다. 실제 또는 별도로 검증된 descriptor response 없이 Function 6이나 `0x458` 출력을 추측하지 않습니다.

*The next task traces the in-place response and status consumer for the `0x9c40244c` Function-0 descriptor. No Function-6 or `0x458` output is guessed without an actual or independently verified descriptor response.*
