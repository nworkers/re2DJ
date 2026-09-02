# Hardlock 우회 스텁 작업 지시

관련 설계: [Hardlock 우회 스텁](../design/20260901-131-hardlock-bypass-stub.md)

*Related design: [Hardlock bypass stub](../design/20260901-131-hardlock-bypass-stub.md).*

## 범위

1. 플랫폼 중립 `re2dj::device::HardlockStubDevice`를 추가해 네 Hardlock IOCTL의 결정적 합성 응답과 계약 위반 거절을 한곳에 모읍니다.
2. injected runtime에 `g_re2dj_hardlock_bypass_enabled`를 추가하고, 활성일 때 `DeviceIoControl` thunk가 스텁으로 요청을 넘기도록 연결합니다. 기존 `HardlockProtocolTracker` 관찰은 그대로 유지합니다.
3. 기존 `--device-mock-hardlock-450-response`, `--device-mock-hardlock-44c-tail` 실험값을 스텁 옵션으로 전달해 분기 실험 경로를 유지합니다.
4. launcher에 `--hardlock-bypass`를 추가하고, 활성화를 JSONL 이벤트로 한 번 보고합니다.
5. 제품 CLI에 `--hardlock-bypass`를 추가하고, Hardlock 설정을 요구하는 profile에서만 허용해 launcher로 전달합니다.
6. unit test로 스텁 계약을 고정하고 Windows x86 build로 검증합니다.
7. 설계·분석·아키텍처 문서를 갱신하고 작업 로그를 남깁니다.

*Add the platform-neutral `re2dj::device::HardlockStubDevice` holding the deterministic synthetic responses and contract rejections for all four Hardlock IOCTLs; add `g_re2dj_hardlock_bypass_enabled` to the injected runtime and route `DeviceIoControl` through the stub when it is set, keeping the existing `HardlockProtocolTracker` observation intact; feed the existing `--device-mock-hardlock-450-response` and `--device-mock-hardlock-44c-tail` experiment values into the stub as options so the branch experiments survive; add `--hardlock-bypass` to the launcher with a single JSONL activation event and to the product CLI, allowed only for profiles that require a Hardlock configuration; pin the stub contract with unit tests and verify with a Windows x86 build; update the design, analysis, and architecture documents and leave a work log.*

## 비범위

- 게스트 보호 분기 결과 강제(2단계). 막히는 분기를 계측으로 특정한 뒤 별도 작업에서 다룹니다.
- Function `0x0e` 변환 구현. 허용 가능한 독립 근거가 없어 항등으로 남깁니다.
- `\\.\NTICE` 강제 성공. 역할이 확정되지 않았습니다.

*Out of scope: stage-2 guest protection-branch forcing, which waits until instrumentation identifies the blocking branch; a Function `0x0e` transform, which stays identity for lack of a policy-compatible independent basis; and forcing `\\.\NTICE` open, whose role is unconfirmed.*

## 완료 조건

- 우회는 기본값으로 꺼져 있고, profile 기본값이 아니라 명시적 옵션으로만 켜집니다.
- 우회가 켜진 실행은 runtime trace와 launcher JSONL 양쪽에서 식별 가능합니다.
- 스텁 로그와 test에 키값, descriptor ID, block payload가 나타나지 않습니다.
- unit test가 네 IOCTL의 성공 계약과 거절 조건을 모두 덮고 전체 test가 통과합니다.
- Windows x86 build가 통과합니다.

*Completion requires the bypass to default to off and be reachable only through an explicit option rather than a profile default; bypassed runs to be identifiable from both the runtime trace and the launcher JSONL; no keys, descriptor IDs, or block payloads in stub logs or tests; unit tests covering every success contract and rejection condition with the full suite passing; and a passing Windows x86 build.*
