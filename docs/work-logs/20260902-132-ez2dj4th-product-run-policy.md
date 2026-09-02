# ez2dj4th 제품 실행 경로 정책 작업 로그

관련 설계: [ez2dj4th 제품 실행 경로 정책](../design/20260902-132-ez2dj4th-product-run-policy.md)
관련 작업 지시: [ez2dj4th 제품 실행 경로 정책](../work-orders/20260902-132-ez2dj4th-product-run-policy.md)

*Related design: [ez2dj4th product run policy](../design/20260902-132-ez2dj4th-product-run-policy.md). Related work order: [ez2dj4th product run policy](../work-orders/20260902-132-ez2dj4th-product-run-policy.md).*

## 결과

- `TargetRunDefaults`에 `hle_wts_active_console`을 추가했습니다. 성공한 `WTS_CURRENT_SESSION` class-4 결과만 active 상태 `0`으로 바꾸는 기존 경로를 그대로 사용합니다.
- `ez2dj4th` profile에 `run_detached`와 `hle_wts_active_console`을 켰습니다. 두 값 모두 이유를 코드 주석에 남겼습니다.
- Windows 인자 생성기가 정책이 켜졌을 때만 `--device-mock-wts-console-session`을 전달합니다. active console 정책이 device 정책 없이 오는 조합은 거절합니다.
- `HasExecutionPolicy`에 새 정책을 포함해, 이 정책만 가진 profile도 실행 경로를 갖도록 했습니다.
- Hardlock 우회는 profile 기본값으로 올리지 않았습니다. `--hardlock-bypass`는 계속 명시적 옵션입니다.

*Added `hle_wts_active_console` to `TargetRunDefaults`, reusing the existing path that rewrites only a successful `WTS_CURRENT_SESSION` class-4 result to active state `0`. Enabled it and `run_detached` for the `ez2dj4th` profile, with the reason for each recorded in a code comment. The Windows argument builder now forwards `--device-mock-wts-console-session` only when the policy is set and rejects an active-console policy without a device policy, and `HasExecutionPolicy` counts the new policy so a profile carrying only it still has an execution path. The Hardlock bypass was not promoted to a profile default and stays the explicit `--hardlock-bypass` option.*

## 검증

- Windows x86 Debug build 통과
- Unit tests: `1125` checks, `0` failures
- 선택 CTest(`re2dj_unit_tests`, `re2dj_windows_hardlock_descriptor_probe`, `re2dj_windows_product_loader_probe`): `3/3` 통과. product-loader probe에 4th 인자 순서, 우회 opt-in, active-console 정책 거절 검사를 추가했습니다.
- 실제 CHD 제품 실행 `re2dj.exe ez2dj4th --run --hardlock-bypass`: launcher JSONL `launch` 이벤트에 `run_detached=true`, `device_mock_wts_console_session=true`가 전달되었습니다. `runtime_detached` 뒤 우회가 실제로 실행되어 initialize 1회와 handshake 3회가 모두 `outcome=completed`로 기록되었고, `runtime_detached_exit code=0x00000008`로 종료했습니다. 이는 옵션 없는 bounded launcher 실행과 같은 지점입니다.
- 실제 CHD 제품 실행 `re2dj.exe ez2dj4th --run`: `hardlock_bypass` 이벤트와 `hardlock-bypass` trace 줄이 0건입니다. detached 실행이 스스로 `0x00000008`로 종료해 CLI가 대기 상태로 남지 않았습니다.
- 두 실행 뒤 잔여 `EZ2DJ.EXE`/`re2dj` 프로세스가 없음을 확인했습니다.
- `git diff --check` 통과

*Verification: the Windows x86 Debug build passes, unit tests report 1,125 checks with zero failures, and selected CTest passes 3/3, with the product-loader probe extended to cover the 4th argument order, bypass opt-in, and rejection of an active-console policy without a device policy. A real-CHD `re2dj.exe ez2dj4th --run --hardlock-bypass` shows `run_detached=true` and `device_mock_wts_console_session=true` in the launcher JSONL `launch` event; after `runtime_detached` the bypass actually runs, recording one initialize and three handshakes all `outcome=completed`, and the child exits with `runtime_detached_exit code=0x00000008` — the same point an option-free bounded launcher run reaches. A real-CHD `re2dj.exe ez2dj4th --run` records no `hardlock_bypass` event and no `hardlock-bypass` trace line, and the detached child exits `0x00000008` on its own so the CLI does not hang. No `EZ2DJ.EXE` or `re2dj` process remained after either run, and `git diff --check` passes.*

## 남은 제약

- 제품 경로는 여전히 `0x44c`에 도달하지 않습니다. 이는 [Task 131](20260901-131-hardlock-bypass-stub.md)이 확인한 `0x450` 응답 blocker이며, 이번 정책 변경의 범위가 아닙니다. `0x44c` 이후를 보려면 launcher의 분기 실험값이 필요하고, 그 값들은 실제 dongle 응답이 아니므로 제품 기본값으로 올리지 않습니다.

*Remaining limits: the product path still does not reach `0x44c`, which is the `0x450` response blocker confirmed in [Task 131](20260901-131-hardlock-bypass-stub.md) and outside this policy change; seeing past `0x44c` needs the launcher's branch-experiment values, which are not real dongle responses and are therefore not promoted to product defaults.*
