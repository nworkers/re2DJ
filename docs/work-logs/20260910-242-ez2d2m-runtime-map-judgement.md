# 작업 로그: ez2d2m 런타임 challenge map 재판별

## 한국어

### 관련 문서

- 분석: [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)의 2026-09-10 재판별 절, [EZ2Dancer I/O 포트 맵](../analysis/ez2dancer-io-map.md)의 원본 확인 절
- 선행 작업: [ez2d2m 후보 map 검증과 판별](20260910-241-ez2d2m-candidate-judgement.md), [ez2d2m Hardlock descriptor 확보](20260910-240-ez2d2m-hardlock-descriptor.md)

### 요구사항

사용자가 교체한 `cfg/ez2d2m/` 산출물을 확인합니다.

코드는 바꾸지 않았습니다. 검증과 관측 작업이므로 작업 지시서 없이 로그와 분석 갱신만 남깁니다.

### 무엇이 바뀌었나

작업 241에서 정적 challenge 목록에 런타임 11번째 challenge가 빠져 있고 쓰이지 않는 항목 3개가 있다고 보고했습니다. 사용자가 런타임 목록으로 map을 재생성해 `cfg/ez2d2m/runtime-maps/`에 두었습니다. 기존 `maps/`는 그대로 보존됐고 후보 index 0–94는 두 디렉터리에서 같습니다.

### 산출물 검증 — 확인됨

map 95개가 각 11행이고, challenge 집합이 `runtime-challenges.txt`와 해시 단위로 일치하며, response 조합 95개가 모두 다르고 형식 위반이 없습니다. 표본 후보 4개에서 구 정적 map과 공통인 challenge 10개의 response가 완전히 동일해 같은 seed에서 재생성됐음이 확인됩니다.

실행에서 `entries=11`로 로드되고 `mapped=11:unmapped=0`이 되어, 작업 241에서 지적한 결함이 해소됐습니다.

### 재판별 — 확인됨

후보 95개를 전수 실행했습니다. 결과는 `cfg/ez2d2m/judgement-sweep-runtime.txt`에 있습니다.

| 부류 | 후보 수 | handshake | descriptor | trace 줄 |
| --- | --- | --- | --- | --- |
| 1차 라운드에서 종료 | 94 | 2 | 12 | 227 또는 229 |
| `candidate-70` | 1 | 4 | 14 | 248 |

분리는 작업 241과 같고, `candidate-70`만 더 멀리 갑니다. 정적 map일 때의 descriptor 13건·240줄이 descriptor 14건·248줄이 됐습니다. 빠졌던 challenge를 채운 효과가 이 후보에서만 나타난다는 점이 판별을 한 번 더 지지합니다.

### 가장 중요한 결과 — 보호를 지났습니다

`candidate-70` 실행의 두 번째 fault가 `0xc0000096`(privileged instruction)이고, 주소 RVA `0x0000b565`가 원본 `.text`(`0x1000`–`0x4c93e`) 안입니다. **게스트가 보호 계층을 지나 원본 게임 코드를 실행하고 있습니다.** 잘못된 map은 여기에 도달할 수 없습니다.

이는 3rd·4th·1st SE·1st·5th에서 확정 후보가 처음 보인 것과 같은 양상입니다. 그 다섯은 트랩되지 않은 byte I/O에서 멈췄고, 여기서는 트랩되지 않은 word I/O에서 멈춥니다.

### 부수 결과 — I/O 맵이 원본에서 확인됐습니다

crash context의 code window를 디코드했습니다. 두 번 실행에서 동일합니다.

| 항목 | 값 |
| --- | --- |
| 명령 바이트 | `66 ef` = `OUT DX, AX` |
| 명령 길이 | 2바이트 (`0x66` prefix 포함) |
| `edx` | `0x030a` |
| `eax` | `0x00000004` |

이로써 [EZ2Dancer I/O 포트 맵](../analysis/ez2dancer-io-map.md)의 두 항목이 추정에서 확인됨으로 올라갔습니다. **16비트 폭 접근**과 **출력 port `0x30a`** 입니다. 그 문서는 작성 시점에 전부 추정이었고 근거가 공개 구현 하나뿐이었으므로, 원본 바이너리에서의 첫 확인입니다. 기록된 값 `0x0004`가 추정 표의 `NEON` bit와 같다는 점도 모순이 없습니다.

작업 239에서 `ez2d2m` 프로파일의 `legacy_io_ports`를 끈 판단이 이 관측으로 뒷받침됩니다. 켜 두었다면 fault handler가 이 `66 ef`를 byte helper로 오인하고 `EIP`를 1만 증가시켜 명령 중간으로 복귀했을 것입니다.

### 아직 아닌 것 — 미확정

- 자산을 아직 열지 못합니다(`asset-open` 0건). 게스트는 자산 로딩 전에 캐비닛 출력을 먼저 건드리고 거기서 멈춥니다.
- `candidate-70`을 정답 map으로 확정하지 않습니다. 원본 `.text` 도달은 매우 강한 증거지만, 이 저장소의 확정 기준은 게스트가 자기 자산을 읽는 것이고 그것은 16비트 I/O 경계가 생긴 뒤에만 확인됩니다.
- 나머지 port와 bit 배치. 입력 helper 위치는 이 출력을 트랩한 뒤에야 관측됩니다.
- 126번째 줄 `.protect` 안의 `0xc0000005`. 실행이 이어지므로 치명적이지 않으나 성격은 미확정입니다.

### 다음 단계

`LegacyIoPortBus`와 privileged fault handler를 word 폭과 `0x300` 대역으로 넓히는 작업입니다. 작업 239 설계의 제외 범위로 남겨 둔 항목이며, 이제 그것이 `ez2d2m`의 유일한 차단 지점입니다. 확인된 출력 helper RVA `0x0000b565`를 그때 사용합니다. 지금의 `legacy_io_*_byte_rva`에 넣으면 안 됩니다. 그 경로는 byte 의미와 1바이트 명령을 전제합니다.

### 검증

| 항목 | 결과 |
| --- | --- |
| 전수 재판별 | 후보 95개 전부 실행, 결과 `cfg/ez2d2m/judgement-sweep-runtime.txt` |
| `candidate-70` 재현성 | 두 번의 추가 실행에서 248줄·handshake 4·descriptor 14 동일 |
| privileged fault 재현성 | 두 실행 모두 RVA `0x0000b565`, `edx=0x030a`, `eax=0x00000004` 동일 |
| 코드 변경 | 없음. `git diff -- src/ tests/ CMakeLists.txt` 비어 있음 |
| 비밀 자료 | map·ID·challenge·sweep 결과 모두 Git이 무시하는 `cfg/` 아래에만 존재 |

## English

### Related documents

- Analysis: the 2026-09-10 re-judgement section of [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md), and the original-confirmation section of [the EZ2Dancer I/O port map](../analysis/ez2dancer-io-map.md)
- Preceding tasks: [ez2d2m candidate artifact verification and judgement](20260910-241-ez2d2m-candidate-judgement.md), [ez2d2m Hardlock descriptor extraction](20260910-240-ez2d2m-hardlock-descriptor.md)

### Requirement

Check the `cfg/ez2d2m/` artifacts the repository owner replaced. No code changed, so this leaves a log and analysis updates rather than a work order.

### What changed

Task 241 reported that the static challenge list was missing the runtime's eleventh challenge and carried three entries that are never requested. The owner regenerated the maps against the runtime list into `cfg/ez2d2m/runtime-maps/`. The existing `maps/` is preserved and candidate indices 0–94 are the same in both.

### Artifact verification — confirmed

The 95 maps carry 11 rows each, their challenge set matches `runtime-challenges.txt` hash for hash, all 95 response sets differ, and no row is malformed. In four sampled candidates the ten challenges shared with the older static maps carry identical responses, confirming regeneration from the same seeds. Runs load them as `entries=11` and report `mapped=11:unmapped=0`, so the defect from task 241 is resolved.

### Re-judgement — confirmed

All 95 candidates were run; results are in `cfg/ez2d2m/judgement-sweep-runtime.txt`. Ninety-four end in the first round with 2 handshakes, 12 descriptors and 227 or 229 lines; `candidate-70` alone reaches 4 handshakes, 14 descriptors and 248 lines, against 13 and 240 with the static maps. That filling the missing challenge helped only this candidate supports the judgement once more.

### The important result — it passes the protection

The second fault in the `candidate-70` run is `0xc0000096`, a privileged instruction, at RVA `0x0000b565`, inside the original `.text` (`0x1000`–`0x4c93e`). **The guest is executing original game code, past the protection layer** — a wrong map cannot get there.

This is the same shape the confirmed candidate first showed on 3rd, 4th, 1st SE, 1st and 5th. Those five stopped on untrapped byte I/O; this one stops on untrapped word I/O.

### A side result — the I/O map is confirmed from the original

Decoding the crash context's code window, identically across two runs: the faulting bytes are `66 ef`, an `OUT DX, AX` two bytes long including its `0x66` prefix, with `edx` = `0x030a` and `eax` = `0x00000004`.

That raises two statements in [the EZ2Dancer I/O port map](../analysis/ez2dancer-io-map.md) from inferred to confirmed: the **16-bit access width** and **output port `0x30a`**. Everything in that document was inferred when written, resting on a single public implementation, so this is its first confirmation against the original binary. The written value `0x0004` also matches the inferred table's `NEON` bit, which is consistent rather than conclusive.

The observation supports task 239's decision to leave `legacy_io_ports` off for the `ez2d2m` profile: with it on, the fault handler would have mistaken this `66 ef` for the byte helper and advanced `EIP` by one into the middle of the instruction.

### What this is not — unresolved

No asset is opened yet: the guest touches cabinet output before asset loading and stops there. `candidate-70` is not declared the resolved map — reaching original `.text` is strong evidence, but this repository's bar is the guest reading its own assets, and that can only be checked once a 16-bit I/O boundary exists. The remaining ports and the bit layout stay unresolved, and the input helper can only be located after this output is trapped. The `0xc0000005` inside `.protect` at line 126 is not fatal, since execution continues past it; its nature is unresolved.

### Next step

Widening `LegacyIoPortBus` and the privileged-fault handler to word width and the `0x300` band — the item task 239's design left out of scope, and now the single thing blocking `ez2d2m`. The confirmed output helper RVA `0x0000b565` belongs to that work; it must not be placed in today's `legacy_io_*_byte_rva`, which assume byte semantics and a one-byte instruction.

### Verification

| Item | Result |
| --- | --- |
| Full re-judgement | All 95 candidates run; results in `cfg/ez2d2m/judgement-sweep-runtime.txt` |
| `candidate-70` reproducibility | Two further runs give the same 248 lines, 4 handshakes and 14 descriptors |
| Privileged fault reproducibility | Both runs give RVA `0x0000b565`, `edx=0x030a`, `eax=0x00000004` |
| Code changes | None; `git diff -- src/ tests/ CMakeLists.txt` is empty |
| Secret material | Maps, IDs, challenges and sweep results live only under the Git-ignored `cfg/` |
