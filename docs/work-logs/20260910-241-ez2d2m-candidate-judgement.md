# 작업 로그: ez2d2m 후보 map 검증과 판별

## 한국어

### 관련 문서

- 분석: [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)의 2026-09-10 후보 map 판별 절
- 절차: [Hardlock descriptor ID 추출](../guides/hardlock-descriptor-extraction.md)
- 선행 작업: [ez2d2m Hardlock descriptor 확보](20260910-240-ez2d2m-hardlock-descriptor.md)
- 유사 선례: [ez2dj1st·ez2dj5th Hardlock descriptor 확보](20260909-237-ez2dj1st-5th-hardlock-descriptors.md)

### 요구사항

사용자가 `cfg/ez2d2m/`에 추가한 후보 산출물을 확인합니다.

코드는 바꾸지 않았습니다. 검증과 관측 작업이므로 작업 지시서 없이 로그와 분석 갱신만 남깁니다.

### 산출물

외부 도구 `resoftlock`이 만든 후보 세트입니다. 후보 map 95개, seed 후보 목록, shard 4개, 정적 identifier 보고서, 생성 절차 README로 구성됩니다. `cfg/`는 Git이 무시하므로 저장소에는 들어오지 않습니다.

### 한 일

1. 산출물의 내부 정합성을 검사했습니다.
2. 산출물의 입력이 이전 작업에서 실행으로 추출한 descriptor와 일치하는지 대조했습니다.
3. 런타임이 실제로 요청하는 challenge를 덤프해 산출물의 정적 목록과 비교했습니다.
4. 후보 95개를 전수 실행해 판별했습니다.

### 결과 — 확인됨

**정합성은 모두 통과했습니다.** 원본 PE SHA-256이 CHD에서 추출한 실행 파일과 일치하고, `module_address` `0x4c5e`와 `id_ref`·`id_verify`가 런타임 추출값과 일치합니다. shard 4개의 후보 수가 95로 합산되고 seed3 탐색이 전 범위를 덮으며, 병합 목록은 고유 triple 95개가 오름차순입니다. map 95개는 각 13행에 형식 위반 0, response 조합 95개 모두 상이하고, 기존 3rd map과 같은 형식이라 launcher가 `entries=13`으로 로드합니다.

**challenge 목록에 결함이 있습니다.** 런타임은 고유 challenge 11개를 요청하는데 정적 목록은 13개입니다. 앞 10개는 순서까지 일치하지만 런타임 11번째가 목록에 없어 모든 실행이 `mapped=10:unmapped=1`이 되고, 목록의 나머지 3개는 한 번도 요청되지 않습니다. 그 중 하나는 값이 main image 주소 형태여서 오탐으로 보입니다.

**판별은 하나로 갈렸습니다.** 빠진 challenge가 마지막 요청이라 앞 10개로도 후보가 갈립니다.

| 부류 | 후보 수 | handshake | descriptor | trace 줄 |
| --- | --- | --- | --- | --- |
| 1차 라운드에서 종료 | 94 | 2 | 12 | 227 또는 229 |
| `candidate-70` | 1 | 4 | 13 | 240 |

`candidate-70`만 2차 보호 라운드에 진입하며, 재실행에서도 같은 수치가 나옵니다. 산출물 README가 기존 다섯 제품의 쌍 B를 근거로 예측한 후보와 일치합니다.

### 정정

표본 4개를 먼저 돌렸을 때 `candidate-70`에 crash가 없다고 사용자에게 보고했습니다. **틀렸습니다.** 전수 실행에서 `candidate-70`도 `0xc0000005`로 한 번 죽습니다. 240줄 중 126번째 줄, 마지막 transform 직후입니다. 다른 후보와의 차이는 crash 유무가 아니라 그 뒤로 실행이 이어져 2차 라운드까지 간다는 점입니다.

원인은 표본 단계에서 두 trace를 diff한 결과의 hunk만 보고 판단한 것입니다. crash 줄이 대체된 것처럼 보였지만 실제로는 파일 뒤쪽으로 밀린 것이었습니다. 줄 번호를 직접 확인했으면 바로 드러났습니다.

### 아직 아닌 것 — 미확정

- 어느 후보도 자산을 열지 못했습니다(`asset-open` 0건).
- `candidate-70`이 정답 map이라고 확정할 수 없습니다. 확정된 것은 95개 중 유일하게 더 진행한다는 사실뿐입니다.
- 126번째 줄의 crash가 빠진 11번째 challenge 때문인지는 확인되지 않았습니다. 위치상 그럴듯하다는 것 이상은 없습니다.

### 다음 단계

seed 후보는 descriptor에서 유도되며 challenge 목록과 독립입니다. 따라서 같은 95개 seed에 런타임 challenge 목록을 넣어 map 생성 단계만 다시 수행하면 됩니다. 런타임 목록은 `cfg/ez2d2m/runtime-challenges.txt`에 있고, 이 단계는 외부 도구 `resoftlock`이 담당하므로 사용자가 수행합니다. 이후 재판별은 이 작업과 같은 방법으로 반복할 수 있습니다.

### 검증

| 항목 | 결과 |
| --- | --- |
| 전수 판별 | 후보 95개 전부 실행, 결과 `cfg/ez2d2m/judgement-sweep.txt` |
| `candidate-70` 재현성 | 재실행에서 240줄·handshake 4·descriptor 13 동일 |
| 코드 변경 | 없음. `git diff -- src/ tests/ CMakeLists.txt` 비어 있음 |
| 비밀 자료 | 후보 map·ID·런타임 challenge 모두 Git이 무시하는 `cfg/` 아래에만 존재 |

## English

### Related documents

- Analysis: the 2026-09-10 candidate-judgement section of [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md)
- Procedure: [Hardlock descriptor ID extraction](../guides/hardlock-descriptor-extraction.md)
- Preceding task: [ez2d2m Hardlock descriptor extraction](20260910-240-ez2d2m-hardlock-descriptor.md)
- Comparable precedent: [ez2dj1st and ez2dj5th Hardlock descriptors](20260909-237-ez2dj1st-5th-hardlock-descriptors.md)

### Requirement

Check the candidate artifacts the repository owner added under `cfg/ez2d2m/`.

No code changed. This is verification and observation, so it leaves a log and an analysis update rather than a work order.

### The artifacts

A candidate set produced by the external `resoftlock` tool: 95 candidate maps, the seed candidate list, four shards, a static identifier report and a README describing generation. `cfg/` is Git-ignored, so none of it enters the repository.

### What was done

The artifacts' internal consistency was checked; their inputs were compared against the descriptor extracted at runtime in the preceding task; the challenges the runtime actually requests were dumped and compared with the static list; and all 95 candidates were run against the original executable.

### Results — confirmed

**Consistency passes throughout.** The original PE SHA-256 matches the executable extracted from the CHD, and `module_address` `0x4c5e` together with `id_ref` and `id_verify` match the runtime-extracted values. The four shards sum to 95 candidates over the full seed3 range, and the merged list holds 95 unique triples in ascending order. The 95 maps carry 13 rows each with no malformed rows and 95 distinct response sets, in the same format as the existing 3rd map, so the launcher loads them as `entries=13`.

**The challenge list is defective.** The runtime requests 11 unique challenges against the list's 13. The first ten match exactly, including order, but the runtime's eleventh is absent, so every run reports `mapped=10:unmapped=1`; the list's three remaining entries are never requested, and one of them has the shape of a main-image address and looks like a false positive.

**Judgement separated exactly one candidate.** Because the missing challenge is the last request, the first ten still discriminate. Ninety-four candidates end in the first round with 2 handshakes, 12 descriptors and 227 or 229 trace lines; `candidate-70` alone reaches 4 handshakes, 13 descriptors and 240 lines, entering a second protection round. A re-run reproduces the figures. It is the candidate the artifact README predicted from the product-family B pair of the five previously judged products.

### Correction

After sampling four candidates, the user was told `candidate-70` did not crash. **That was wrong.** In the full sweep `candidate-70` also dies once with `0xc0000005`, at line 126 of its 240, immediately after the last transform. What separates it from the others is not the absence of a crash but that execution continues afterwards into a second round.

The error came from reading only the hunk of a diff between two traces at the sampling stage: the crash lines appeared to be replaced when they had in fact moved later in the file. Checking the line number directly would have shown it at once.

### What this is not — unresolved

No candidate opened any asset. `candidate-70` cannot be declared the correct map; what is established is only that it alone progresses further. Whether the crash at line 126 is caused by answering the missing eleventh challenge wrongly is not established — the position makes it plausible and nothing more.

### Next step

Seed candidates derive from the descriptor and are independent of the challenge list, so the same 95 seeds only need the map-generation step repeated against the runtime challenge list, which is kept at `cfg/ez2d2m/runtime-challenges.txt`. That step belongs to the external `resoftlock` tool and is the owner's to run; re-judging afterwards repeats the method used here.

### Verification

| Item | Result |
| --- | --- |
| Full judgement | All 95 candidates run; results in `cfg/ez2d2m/judgement-sweep.txt` |
| `candidate-70` reproducibility | A re-run gives the same 240 lines, 4 handshakes and 13 descriptors |
| Code changes | None; `git diff -- src/ tests/ CMakeLists.txt` is empty |
| Secret material | Candidate maps, IDs and runtime challenges all live only under the Git-ignored `cfg/` |
