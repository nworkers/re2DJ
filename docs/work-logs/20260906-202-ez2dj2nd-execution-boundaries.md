# ez2dj2nd 실행 경계 보정 작업 로그

## 결과 요약

`ez2dj2nd`가 launcher 준비 단계에서 실패하거나 privileged I/O fault로 즉시 종료되는 경로를 보정했습니다. 2nd 프로파일에서 지원되지 않는 demo-volume 주입을 제거하고, 런타임 진단으로 확인한 input/output helper RVA를 적용했습니다. 또한 2nd를 포함한 4th 이외 타깃에 `DirectDrawCreateEx` HLE IAT patch를 적용하도록 launcher 정책을 수정했습니다.

The launcher preparation and privileged-I/O paths that stopped `ez2dj2nd` were corrected. The unsupported demo-volume injection was removed from the 2nd profile, the runtime-confirmed input/output helper RVAs were applied, and the launcher now patches the `DirectDrawCreateEx` HLE IAT for non-4th targets including 2nd.

## 변경 내용

- `ez2dj2nd` demo-volume 기본값 제거
- legacy input helper RVA `0x000782d7` 적용
- legacy output helper RVA `0x0007832b` 적용
- non-4th 타깃의 `DirectDrawCreateEx` IAT patch 활성화
- `preparation_status` diagnostic event 추가
- launcher의 빈 오류 문자열에 준비 단계별 fallback 원인 추가
- 2nd 프로파일 단위 테스트와 product-loader argument 계약 테스트 보강

## 확인된 증거

- `logs/windows_x86_launcher_probe/ez2dj2nd/20260906-020151-651.jsonl`: 초기 프로파일의 demo-volume 요구가 2nd의 `GetPrivateProfileIntA` import 부재와 충돌함.
- `logs/windows_x86_launcher_probe/ez2dj2nd/20260906-022613-342.jsonl`: input `IN AL,DX` helper RVA `0x000782d7`와 output `OUT DX,AL` helper RVA `0x0007832b`가 순차적으로 관찰됨.
- `logs/windows_x86_launcher_probe/ez2dj2nd/20260906-022933-169.jsonl`: 두 helper 주소에서 2,643회의 privileged I/O trap이 모두 first-chance로 처리되었고 second-chance fault는 없음.
- `logs/windows_x86_launcher_probe/ez2dj2nd/20260906-023202-123.jsonl`: 최종 프로파일에서 `runtime_detached`가 기록되고 프로세스가 살아 있는 상태까지 진행됨. 검증 후 남은 게임 프로세스는 수동 종료함.
- `logs/windows_x86_launcher_probe/ez2dj2nd/20260906-022550-689.ddraw.log`: `DirectDrawCreateEx` HLE 경로가 실제 호출됨.

## 검증

- Windows x86 Debug build: 통과
- `re2dj_unit_tests.exe`: `checks: 1321, failures: 0`
- `re2dj_windows_product_loader_probe.exe`: `profile-defaults=ok second-defaults=ok unsupported-target=ok resolve-iat-slot=ok`
- CTest 선택 실행: 2/2 통과
- 시각적 music-select 결과와 2nd Hardlock 동작은 원본 자산과 실행 환경에 의존하므로 이 작업에서 확정하지 않음.

## 남은 미확정 사항

2nd의 Hardlock 계약, 실제 물리 I/O 장치 동작, 그리고 music-select 화면의 최종 시각적 일치 여부는 별도 실행 검증이 필요합니다. 이번 작업은 해당 로직을 추정해 재구현하지 않고, 관찰된 실행 경계만 프로파일과 HLE 연결부에 반영했습니다.

The 2nd Hardlock contract, physical I/O behavior, and final visual equivalence of the music-select screen remain unresolved and require separate runtime verification. This task applies only observed execution-boundary facts and does not reconstruct those game or device behaviors.
