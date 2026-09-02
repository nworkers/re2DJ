# ez2dj4th 제품 실행 경로 정책 작업 지시

관련 설계: [ez2dj4th 제품 실행 경로 정책](../design/20260902-132-ez2dj4th-product-run-policy.md)

*Related design: [ez2dj4th product run policy](../design/20260902-132-ez2dj4th-product-run-policy.md).*

## 범위

1. `TargetRunDefaults`에 `hle_wts_active_console`을 추가합니다.
2. `ez2dj4th` profile에 `run_detached`와 `hle_wts_active_console`을 켭니다.
3. Windows 인자 생성기가 정책이 켜졌을 때만 `--device-mock-wts-console-session`을 전달하도록 연결합니다.
4. unit test로 profile 값을 고정하고, 인자 생성 계약은 `re2dj_windows_product_loader_probe`로 고정합니다.
5. Windows x86 build와 실제 CHD 제품 실행으로 검증합니다.
6. 설계·아키텍처·분석 문서를 갱신하고 작업 로그를 남깁니다.

*Add `hle_wts_active_console` to `TargetRunDefaults`, enable it and `run_detached` for the `ez2dj4th` profile, make the Windows argument builder pass `--device-mock-wts-console-session` only when that policy is set, pin the profile values with unit tests and the argument contract with `re2dj_windows_product_loader_probe`, verify with a Windows x86 build and a real-CHD product run, and update the design, architecture, and analysis documents with a work log.*

## 비범위

- Hardlock 우회의 기본 활성화. `--hardlock-bypass`는 명시적 옵션으로 유지합니다.
- 다른 profile의 정책 변경.

*Out of scope: enabling the bypass by default, which stays the explicit `--hardlock-bypass` option, and changing other profiles' policies.*

## 완료 조건

- `ez2dj4th` 제품 실행이 첫 `CreateFileA` 이후로 진행합니다.
- WTS 정책이 없는 profile에는 `--device-mock-wts-console-session`이 붙지 않습니다.
- 우회는 여전히 기본 비활성이며 명시적 옵션으로만 켜집니다.
- unit test 전체와 Windows x86 build가 통과합니다.

*Completion requires the `ez2dj4th` product run to continue past the first `CreateFileA`, no `--device-mock-wts-console-session` for profiles without the policy, the bypass still defaulting to off behind its explicit option, and a passing unit-test suite and Windows x86 build.*
