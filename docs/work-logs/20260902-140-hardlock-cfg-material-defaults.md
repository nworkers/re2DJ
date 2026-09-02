# Hardlock cfg 재료 기본값 작업 로그

관련 설계: [Hardlock cfg 재료 기본값](../design/20260902-140-hardlock-cfg-material-defaults.md), 관련 작업 지시: [작업 지시](../work-orders/20260902-140-hardlock-cfg-material-defaults.md)

*Related design: [Hardlock cfg material defaults](../design/20260902-140-hardlock-cfg-material-defaults.md); related work order: [work order](../work-orders/20260902-140-hardlock-cfg-material-defaults.md).*

## 결과 — 제품 실행 경로가 보호를 통과한다

옵션 없이 실행한 두 제품이 [Task 139](20260902-139-hardlock-candidate-judgement.md)의 통과 지문을 그대로 냅니다.

```
re2dj.exe ez2dj3rd
re2dj.exe ez2dj4th --run
```

| | ez2dj3rd | ez2dj4th |
| --- | --- | --- |
| 적용된 재료 | `response450`, `tail44c`, map | 같음 |
| 매핑 항목 | 28 | 32 |
| transform 주입 | `mapped=32:unmapped=0` | `mapped=36:unmapped=0` |
| vfs trace | 377줄 | 468줄 |
| `EZ2DJ.ini` 열기 | 2회 | 2회 |
| 종료 | `0x00000000` | `0x00000000` |

*The result — the product execution path passes the protection. Run with no options, both products reproduce Task 139's passing fingerprint: the material is applied, injection is complete, the trace grows to its passing length, `EZ2DJ.ini` is opened, and the run exits `0x00000000`.*

## 구현

- `HardlockSecretMaterial`에 `response450`과 `tail44c`을 선택 항목으로 추가했습니다. 값은 launcher 옵션과 같은 hex 문자열 그대로 보관하고, 검증은 기존 파서 하나가 담당합니다. 두 소스가 같은 규칙을 쓰게 하려는 것입니다.
- 관대한 로더 `LoadHardlockProfileMaterial`을 추가했습니다. 파일이나 section이 없으면 실패가 아니라 `found=false`입니다. 다만 scalar 키가 일부만 있으면 오류로 둡니다. module address 없는 seed는 조용히 다른 재료를 고르게 만들기 때문입니다.
- 규약 경로 `DefaultHardlockTransformMapPath(profile_id)` → `cfg/hardlock-<profile-id>.map`을 추가했습니다.
- `TargetLptdiPolicy::hardlock_cfg_material_default`를 추가하고 3rd와 4th에 켰습니다.
- launcher 진입점에서 target 확정 직후 기본값을 해석합니다. 제품 CLI가 같은 진입점을 호출하므로 두 경로가 한 규칙을 공유합니다.
- Hardlock hex/매핑 파싱을 lambda로 묶어 target 확정 이후로 미뤘습니다. 기본값이 채운 값도 같은 파서를 지나게 하려는 것입니다.

*Implementation: `response450` and `tail44c` became optional entries in `HardlockSecretMaterial`, stored as the same hex text the launcher options accept so one existing parser validates both sources; `LoadHardlockProfileMaterial` reads leniently, treating a missing file or section as `found=false` while still rejecting a partial scalar set, since seeds without a module address would silently select different material; `DefaultHardlockTransformMapPath(profile_id)` defines `cfg/hardlock-<profile-id>.map`; `TargetLptdiPolicy::hardlock_cfg_material_default` was added and enabled for 3rd and 4th; the launcher entry point resolves the default right after the target is fixed, and because the product CLI calls that same entry point both paths share one rule; and the Hardlock hex and map parsing moved into a lambda deferred until after target resolution so values filled in by a default pass through the same parser.*

## 전부-또는-전무로 바꾼 이유

처음 구현은 매핑이 없어도 `response450`과 `tail44c`을 적용했습니다. 그 상태로 3rd를 실행하면 보호가 **모달 대화상자를 띄우고 거기서 무한정 기다립니다.** 재료가 없을 때보다 나쁜 상태입니다.

그래서 셋을 함께 적용하도록 바꿨습니다. 매핑이 선택되지 않으면 ini 값도 쓰지 않습니다. 확인 결과 매핑을 치운 3rd 실행은 대화상자 없이 예전 지점인 종료 코드 `0x00000008`로 끝났습니다.

*Why it became all-or-nothing: the first implementation applied `response450` and `tail44c` even without a map, and a 3rd run in that state left the protection at a modal dialog waiting indefinitely — worse than having no material at all. The three are now applied together, so the ini values go unused when no map is selected; with the map removed, a 3rd run ends at the earlier boundary with exit code `0x00000008` and no dialog.*

## 3rd profile 기본값 변경

3rd 제품 경로가 보호 장치에 도달하려면 두 가지가 필요했습니다.

| 기본값 | 근거 |
| --- | --- |
| `hle_dynamic_vfs` | 3rd는 `GetProcAddress`로 해석한 `CreateFileA`로 장치를 엽니다. 꺼져 있으면 device mock이 그 open을 보지 못합니다. [Task 138](20260902-138-ez2dj3rd-transform-challenge-observation.md)이 유보했던 결정입니다 |
| `hle_wts_active_console` | 3rd 보호 초기화가 session connect state를 읽고, active console이면 `0x9c402468`에서 `0x9c402450`으로 진행합니다. 이미 확인된 사실이었고 profile 기본값만 없었습니다 |

`hle_wts_active_console`을 3rd에 금지하던 unit test 주장을 근거에 맞게 갱신했습니다. 이전 주석은 이 정책이 4th에서만 확인되었다고 적었지만, 3rd에 대한 확인 기록이 이미 `ARCHITECTURE.md`에 있었습니다.

*3rd profile default changes: reaching the protection device on 3rd's product path needed `hle_dynamic_vfs`, because 3rd opens its device through a `GetProcAddress`-resolved `CreateFileA` that the device mock cannot see while the resolver is off — the decision Task 138 deferred — and `hle_wts_active_console`, because 3rd's protection initialization reads the session connect state and an active console advances execution from `0x9c402468` to `0x9c402450`, which was already confirmed and merely missing as a profile default. The unit-test assertion that forbade `hle_wts_active_console` on 3rd was updated to match that evidence; its old comment claimed the policy was confirmed for 4th only, while the confirmation for 3rd was already recorded in `ARCHITECTURE.md`.*

## 정책

- 합성 `0x450`/`0x44c` 값을 코드 상수로 올리지 않았습니다. 세 재료 모두 사용자가 `cfg/`에 두는 파일에서만 옵니다. 저장소에는 경로 규칙과 읽는 코드만 있습니다.
- re2DJ는 여전히 응답을 계산하지 않습니다. Function `0x0e` 변환은 구현하지 않았고 링크하지도 않습니다.
- 진단 로그에는 어떤 재료가 적용됐는지만 남고 값은 남지 않습니다.
- `/cfg/`는 이미 ignore 대상이라 승격한 매핑 파일 두 개가 Git에 나타나지 않는 것을 확인했습니다.

*Policy: no synthetic `0x450` or `0x44c` value was promoted to a code constant — all three materials come only from files the user places under `cfg/`, and the repository carries just the path convention and the reading code; re2DJ still computes no response, with the Function `0x0e` transform neither implemented nor linked; the diagnostic log records which material was applied and never its values; and `/cfg/` is already ignored, which was verified for the two promoted map files.*

## 검증

- Windows x86 Debug build 통과, 경고 없음
- unit test `checks: 1216, failures: 0`
- 재료 있음: 두 제품 모두 통과 지문 재현
- 재료 없음(매핑 제거): 3rd가 종료 코드 `0x00000008`로 예전 지점에서 정지, `hardlock_cfg_material` 이벤트 없음
- 잔여 원본 process 없음. 원본 HDD, CHD, overlay 변경 없음
- `git status`에 `cfg/` 항목 없음

*Verification: the Windows x86 Debug build passed without warnings; unit tests reported `checks: 1216, failures: 0`; with material present both products reproduced the passing fingerprint; with the map removed, 3rd stopped at the earlier boundary with exit code `0x00000008` and no `hardlock_cfg_material` event; no original process remained and the original HDD, CHD, and overlay are unchanged; and `git status` shows nothing under `cfg/`.*

## 다음 단계

1. `MapVfsPath`가 절대 host 경로를 그대로 통과시키도록 고칩니다. 두 제품이 지금 `EZ2DJ.ini`에서 막혀 있습니다.
2. 그 뒤 실행이 어디까지 가는지 다시 측정합니다. 지금의 정상 종료는 성공이 아니라 설정을 읽지 못한 결과로 추정됩니다.
3. launcher probe에 게스트 memory dump를 추가해 [Task 137](20260902-137-decrypted-region-judge.md) 판별기로 복호화된 `.text`를 직접 채점합니다.

*Next: fix `MapVfsPath` so an absolute host path passes through, since both products are now blocked at `EZ2DJ.ini`; re-measure how far execution reaches afterwards, because the current clean exit is inferred to be a failure to read configuration rather than success; and add a guest memory dump to the launcher probe so the Task 137 judge can score the decrypted `.text` directly.*
