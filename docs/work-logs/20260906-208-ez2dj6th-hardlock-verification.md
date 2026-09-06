# ez2dj6th Hardlock 후보 검증 작업 로그

## 결과

`cfg/ez2dj6th/maps/candidate-*.map` 194개를 모두 `EZ2DJ/EZ2DJ.EXE` bootstrap에서 시작해 `EZ2DJ6th.EXE` child까지 추적하는 동일 조건으로 실행했습니다.

모든 후보에서 다음 실행 형태가 동일했습니다.

- child 생성 및 child handoff 확인
- `0x450` handshake 2회
- `0x44c` descriptor 1회
- `0x458` transform 0회
- timeout 0회

따라서 이번 실행은 후보 간의 복호화 성공/실패를 판정한 것이 아닙니다. 모든 후보가 동일하게 transform 단계 이전에서 종료했으므로, 현재 blocker는 candidate map이 아니라 6th 전용 초기 Hardlock 응답 경계입니다.

대표 로그는 `logs/windows_x86_launcher_probe/ez2dj6th/20260906-145955-913.jsonl`이며, child-follow와 handoff 상태는 확인되지만 transform event는 없습니다.

*Result*

All 194 files under `cfg/ez2dj6th/maps/candidate-*.map` were run from the `EZ2DJ/EZ2DJ.EXE` bootstrap while following the `EZ2DJ6th.EXE` child under identical conditions.

Every candidate produced the same shape:

- child creation and child handoff observed
- two `0x450` handshakes
- one `0x44c` descriptor
- zero `0x458` transforms
- zero timeouts

This does not judge candidate decryption success or failure. Since every candidate stopped before transform identically, the current blocker is the 6th-specific initial Hardlock response boundary, not the candidate map.

Representative log: `logs/windows_x86_launcher_probe/ez2dj6th/20260906-145955-913.jsonl`. It confirms child-follow and handoff, but contains no transform event.

## 확인 상태

- **확인됨:** bootstrap → child process path and runtime handoff.
- **확인됨:** 6th descriptor identity is reached and parsed.
- **확인됨:** all 194 candidate maps load without map-format failure.
- **미확정:** any candidate seed or `0x458` response.
- **미확정:** the actual 6th `0x450` six-byte driver response.
- **미확정:** the actual Function-0 `0x44c` 256-byte driver response.

*Status*

- **Confirmed:** bootstrap-to-child path and runtime handoff.
- **Confirmed:** the 6th descriptor identity is reached and parsed.
- **Confirmed:** all 194 candidate maps load without a map-format failure.
- **Unresolved:** every candidate seed or `0x458` response.
- **Unresolved:** the actual 6th six-byte `0x450` driver response.
- **Unresolved:** the actual 256-byte Function-0 `0x44c` driver response.

## 다음 작업

실제 6th 장치 응답 또는 원본 실행의 독립적인 oracle이 필요합니다. 초기 handshake/descriptor 응답을 확인해 child가 `0x458`을 호출하도록 만든 뒤에야 194개 후보 map을 유효하게 비교할 수 있습니다. 현재 4th에서 사용한 branch-forcing 값은 이 증거로 사용할 수 없습니다.

*Next step*

An actual 6th device response or an independent oracle from the original execution is required. Only after the initial handshake/descriptor response is confirmed and the child reaches `0x458` can the 194 candidate maps be meaningfully compared. The 4th branch-forcing values cannot serve as that evidence.

## 정정 — 올바른 CHD 포함 재실행

위 결과는 초기 직접 launcher 실행에서 `--chd`가 빠진 실행과, 그 뒤의 올바른 CHD 실행을 구분하지 못한 상태의 기록입니다. 올바른 CHD를 명시한 재실행에서는 194개 후보 모두 child의 `0x458` transform까지 도달했습니다. 실제 descriptor function은 `0x0011`이고 입력은 7개 block이며, 기존 map 기준으로는 모든 block이 `unmapped`였습니다. 따라서 초기 기록의 “transform 0회” 결론은 폐기하고, seed 후보는 아직 판정하지 않습니다.

## Correction — rerun with the correct CHD

The result above mixed an initial direct-launcher run that omitted `--chd` with the later correct CHD run. When the CHD was supplied, all 194 candidates reached the child `0x458` transform. The observed descriptor function was `0x0011` with seven input blocks, and every block was `unmapped` against the existing maps. The earlier “zero transforms” conclusion is superseded; the seed candidates remain unjudged.
