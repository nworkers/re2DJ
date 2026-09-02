# Hardlock cfg 재료 기본값 작업 지시

관련 설계: [Hardlock cfg 재료 기본값](../design/20260902-140-hardlock-cfg-material-defaults.md)

*Related design: [Hardlock cfg material defaults](../design/20260902-140-hardlock-cfg-material-defaults.md).*

## 범위

1. `HardlockSecretMaterial`에 선택 키 두 개를 추가합니다. `response450`과 `tail44c`이며, 값은 launcher 옵션과 같은 hex 문자열 그대로 보관합니다.
2. 파일과 section이 없어도 실패가 아닌 관대한 로더를 추가합니다. profile 기본값이 쓰는 경로입니다.
3. profile 규약 경로 `cfg/hardlock-<profile-id>.map`을 정의합니다.
4. `TargetLptdiPolicy`에 `hardlock_cfg_material_default`를 추가하고 3rd와 4th에 켭니다.
5. launcher 진입점에서 profile 기본값을 해석합니다. 명시적 옵션이 우선하고, 재료는 전부-또는-전무로 적용합니다.
6. 3rd profile에 `hle_dynamic_vfs`와 `hle_wts_active_console`을 켭니다. 둘 다 없으면 3rd 제품 경로가 보호 장치에 도달하지 못합니다.
7. unit test로 새 키, 관대한 로더, 규약 경로, profile 기본값을 고정합니다.
8. 두 제품의 제품 실행 경로로 검증합니다. 재료가 있을 때와 없을 때 모두 확인합니다.
9. 문서를 갱신하고 작업 로그를 남깁니다.

*Add the two optional `response450` and `tail44c` keys to `HardlockSecretMaterial`, stored as the same hex text the launcher options accept; add a lenient loader for which a missing file or section is not a failure, which is what a profile default needs; define the convention path `cfg/hardlock-<profile-id>.map`; add `hardlock_cfg_material_default` to `TargetLptdiPolicy` and enable it for 3rd and 4th; resolve the profile default in the launcher entry point, with an explicit option taking precedence and the material applied all-or-nothing; enable `hle_dynamic_vfs` and `hle_wts_active_console` on the 3rd profile, without which 3rd's product path cannot reach the protection device; pin the new keys, the lenient loader, the convention path, and the profile defaults with unit tests; verify through both products' product execution paths with the material present and absent; and update the documents with a work log.*

## 비범위

- 합성 `0x450`/`0x44c` 값을 코드 상수로 두는 일. 값은 `cfg/`에서만 옵니다.
- Function `0x0e` 변환 구현.
- 보호 통과 이후의 HLE 공백.

*Out of scope: placing the synthetic `0x450` and `0x44c` values in code as constants, since they come only from `cfg/`; implementing the Function `0x0e` transform; and the HLE gaps after the protection.*

## 완료 조건

- 재료가 없으면 두 제품의 제품 경로가 이전과 같은 지점에서 멈춥니다.
- 재료가 있으면 제품 경로가 Task 139의 통과 지문을 냅니다.
- 부분 재료로 모달 대화상자에 멈추지 않습니다.
- 명시적 CLI 옵션이 profile 기본값보다 우선합니다.
- 값이 로그에 남지 않고 저장소에 들어가지 않습니다.
- unit test와 Windows x86 build가 통과합니다.

*Completion requires both product paths stopping where they did before when the material is absent, reproducing Task 139's passing fingerprint when it is present, never stalling at a modal dialog on partial material, an explicit CLI option outranking the profile default, no values in the log or the repository, and passing unit tests and Windows x86 build.*
