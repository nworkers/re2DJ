# 작업 로그: ez2dj1st·ez2dj5th Hardlock descriptor 확보

## 한국어

### 관련 문서

- 설계: [ez2dj1st·ez2dj5th Hardlock descriptor 확보 설계](../design/20260909-237-ez2dj1st-5th-hardlock-descriptors.md)
- 작업 지시: [ez2dj1st·ez2dj5th Hardlock descriptor 확보](../work-orders/20260909-237-ez2dj1st-5th-hardlock-descriptors.md)
- 분석: [ez2dj1st·ez2dj5th Hardlock descriptor 관측](../analysis/ez2dj1st-5th-hardlock-descriptors.md)
- 절차: [Hardlock descriptor ID 추출](../guides/hardlock-descriptor-extraction.md), [Hardlock seed 복구 워크스루](../guides/hardlock-seed-recovery-walkthrough.md)
- 선행 작업: [ez2dj1stse Hardlock 후보 판별](20260908-224-ez2dj1stse-hardlock-candidate-judgement.md)

### 수행 내용

1. 두 실행 파일의 PE 배치를 확인해 `.protect` 계열임을 확정했습니다.
2. descriptor 관측 경로를 확보해 `module_address`, `id_ref`, `id_verify`를 추출했습니다.
3. 관측이 뒤집은 `ez2dj1st` 실행 기본값을 정정했습니다.
4. 사용자가 준비한 후보 map을 두 제품 모두 전수 판별해 각각 하나로 갈랐습니다.
5. 판별 과정에서 드러난 두 제품의 레거시 I/O helper RVA를 실제 코드에서 읽어 정정했습니다.
6. 확정 자료를 3rd·4th·1st SE와 같은 형태로 배치하고, 프로파일 기본값만으로 소비되는지 확인했습니다.

### 두 target은 이미 있었습니다

`ez2dj1st`와 `ez2dj5th`는 built-in profile 표에 이미 있었습니다. `ez2dj1st`는 정정 이전 1st SE의 실행 기본값을, `ez2dj5th`는 4th 호환 기준을 물려받은 상태였습니다. 이 작업은 새로 만드는 대신 두 항목을 관측에 맞게 정정했습니다.

### descriptor에 도달하기

[추출 절차](../guides/hardlock-descriptor-extraction.md)를 그대로 쓰면 두 실행 모두 descriptor에 도달하지 못합니다. 두 가지가 더 필요했습니다.

`--hle-dynamic-vfs`가 없으면 `CreateFileA`가 `route=win32`로 해석되어 게스트가 호스트의 실제 장치를 열려 하고 Hardlock 요청 0건으로 종료합니다. `.protect` packer가 unpack 시 원본 import를 스스로 해석해 정적 IAT 패치를 덮어쓰기 때문이며, 1st SE에서 확인된 것과 같은 이유입니다.

handshake 응답도 필요합니다. 없으면 게스트가 initialize 2회, handshake 3회 뒤 descriptor를 요청하지 않고 종료합니다. [작업 109](../work-logs/20260901-109-ez2dj3rd-hardlock-450-response.md)의 synthetic oracle을 재생해 descriptor 도달 인과성을 만들었습니다.

### 추출 결과 — 확인됨

| 항목 | `ez2dj1st` | `ez2dj5th` |
| --- | --- | --- |
| `header_valid` | 1 | 1 |
| `module_address` | `0x15e5` | `0x4c5c` |
| `id_ref`·`id_verify` | non-zero, 요청 간 동일 | non-zero, 요청 간 동일 |

`module_address`가 계열을 그대로 드러냅니다. 1st는 1st SE(`0x15e1`)와 같은 대역, 5th는 6th(`0x4c51`)와 같은 대역입니다.

**원문 ID는 저장소에 기록하지 않았습니다.** `cfg/hardlock-id.ini`의 `[ez2dj1st]`·`[ez2dj5th]` section에만 있으며, 기존 `[ez2dj6th]`·`[ez2dj1stse]` section은 보존되었습니다.

### 후보 판별 — 확인됨

두 제품 모두 정확히 하나로 갈렸습니다.

| 제품 | 후보 수 | 확정 후보 | 갈린 근거 |
| --- | --- | --- | --- |
| `ez2dj1st` | 134 | `candidate-84` | descriptor 17 대 16, 275줄 대 245/247줄 |
| `ez2dj5th` | 176 | `candidate-135` | descriptor 40 대 38, 363줄 대 329/331줄 |

첫 판별에서는 확정 후보도 자산을 열지 못하고 `0xc0000096`(privileged instruction)으로 멈췄습니다. 트랩되지 않은 port I/O이며, 두 프로파일 모두 다른 빌드에서 물려받은 helper RVA를 쓰고 있었기 때문입니다.

crash 지점의 code window를 디코드해 각 빌드의 helper 쌍을 얻었습니다.

| 제품 | 이전 (물려받음) | 정정 |
| --- | --- | --- |
| `ez2dj1st` | `0x00038987` / `0x000389ab` (1st SE `.gtide`) | `0x00035757` / `0x0003577b` |
| `ez2dj5th` | `0x000c3817` / `0x000c384b` (4th) | `0x000ca067` / `0x000ca09b` |

runtime은 예외 주소를 RVA와 비교하므로 helper 진입점이 아니라 `in`/`out` opcode 바이트의 주소를 씁니다.

정정 후 재실행하면 두 후보 모두 게스트가 자기 자산을 읽습니다.

| 제품 | `.vfs.log` 줄 수 | 자산 개방 |
| --- | --- | --- |
| `ez2dj1st` | 275 → 1875 | 0 → 484 (고유 248) |
| `ez2dj5th` | 363 → 2493 | 0 → 67 |

오답 후보가 우연히 원본 자산의 실제 경로를 만들어 낼 수 없으므로 두 map 모두 확정입니다.

`response450`과 `tail44c`는 3rd·4th·1st SE에서 쓰던 값이 두 제품에서도 그대로 성립합니다. **다섯 제품이 같은 handshake 재생값을 씁니다.**

### 코드 변경

`ez2dj1st` built-in profile:

| 항목 | 이전 | 정정 |
| --- | --- | --- |
| `hle_windows_directory` | `true` | `false` |
| `hle_dynamic_vfs` | 미설정 | `true` |
| `device_mock_path_prefix` | `\\.\LPTDI` | `\\.\FEnteDev` |
| `device_mock_target_state_hex` | `0900000000000000` | 해제 |
| `hardlock_cfg_material_default` | 미설정 | `true` |
| `legacy_io_in_byte_rva` | `0x00038987` | `0x00035757` |
| `legacy_io_out_byte_rva` | `0x000389ab` | `0x0003577b` |

`ez2dj5th` built-in profile은 4th 호환 기준을 유지하되 자기 helper RVA와 관측에 맞춘 note를 갖도록 했습니다. 이를 위해 `MakeChdCompatibilityProfile` 호출을 6th와 같은 블록 형태로 바꿨습니다.

`tests/unit/target_profile_test.cpp`의 CHD 호환 프로파일 검사는 helper RVA 쌍을 인자로 받도록 바꿨습니다. 빌드마다 helper 위치가 다르므로 하나의 기준값을 공유할 수 없습니다.

### 배치한 자료

저장소가 아니라 Git이 무시하는 `cfg/` 아래에만 둡니다.

| 경로 | 내용 |
| --- | --- |
| `cfg/hardlock-ez2dj1st.map` | 확정된 challenge-response map |
| `cfg/hardlock-ez2dj5th.map` | 확정된 challenge-response map |
| `cfg/hardlock.ini` `[ez2dj1st]`·`[ez2dj5th]` | `response450`, `tail44c` |
| `cfg/hardlock-id.ini` `[ez2dj1st]`·`[ez2dj5th]` | `module_address`, `id_ref`, `id_verify` |

### 검증

- Windows x86 Release 전체 build 성공
- `re2dj_unit_tests.exe` → `checks: 1424, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → 4개 항목 ok
- `re2dj ez2dj1st`가 추가 옵션 없이 실행되어 preparation 전 항목 true, `iat_verified=true`, outcome `success`, `hardlock_cfg_material` 세 항목 인식, transform 15건, 자산 484건
- `ez2dj5th`는 Hardlock 옵션 없이 프로파일 기본값만으로 `hardlock_cfg_material` 세 항목 인식, transform 37건, 자산 71건

### 검증하지 못한 항목

`re2dj ez2dj5th` 제품 경로는 `CHD MBR has no in-range FAT32 partition`으로 실패합니다. 공급된 `ez2dj5.chd`는 파티션 테이블이 없는 whole-disk FAT32 볼륨이고 LBA 0 자체가 FAT32 boot sector이므로, 현재 reader가 인식하지 못합니다. 이 작업의 5th 관측은 모두 추출된 디렉터리를 source root로 써서 수행했으며, 실행한 것은 5th의 실제 실행 파일입니다.

### 남은 과제

- ~~파티션 테이블 없는 whole-disk FAT32 볼륨 지원~~ → [작업 238](20260910-238-partitionless-fat32-volume.md)에서 해결했습니다. `ez2dj5th`가 CHD에서 실행됩니다.
- 두 제품의 화면 출력 확인. 이 작업은 Hardlock과 I/O 경계까지만 다뤘습니다.
- 실제 dongle `response450` 확보. 현재 값은 3rd에서 확인된 synthetic oracle입니다.

## English

### Related documents

- Design: [ez2dj1st and ez2dj5th Hardlock Descriptor Design](../design/20260909-237-ez2dj1st-5th-hardlock-descriptors.md)
- Work order: [ez2dj1st and ez2dj5th Hardlock Descriptors](../work-orders/20260909-237-ez2dj1st-5th-hardlock-descriptors.md)
- Analysis: [ez2dj1st and ez2dj5th Hardlock Descriptor Observations](../analysis/ez2dj1st-5th-hardlock-descriptors.md)
- Procedures: [Hardlock descriptor ID extraction](../guides/hardlock-descriptor-extraction.md), [Hardlock seed recovery walkthrough](../guides/hardlock-seed-recovery-walkthrough.md)
- Preceding task: [ez2dj1stse Hardlock candidate judgement](20260908-224-ez2dj1stse-hardlock-candidate-judgement.md)

### Both targets already existed

`ez2dj1st` and `ez2dj5th` were already in the built-in profile table, `ez2dj1st` carrying the pre-correction 1st SE execution defaults and `ez2dj5th` the 4th compatibility baseline. This task corrected the two entries against observation rather than creating them.

### Reaching the descriptor

The [extraction procedure](../guides/hardlock-descriptor-extraction.md) as written reaches no descriptor for either executable; two additions were needed.

Without `--hle-dynamic-vfs`, `CreateFileA` resolves at `route=win32`, the guest opens the host's real device and exits with no Hardlock request at all — the `.protect` packer resolves the original imports itself at unpack time and overwrites the static IAT patch, the same reason established for 1st SE.

A handshake response is needed too: without one the guest exits after two initialize and three handshake requests without ever asking for the descriptor. Replaying the synthetic oracle from [task 109](../work-logs/20260901-109-ez2dj3rd-hardlock-450-response.md) establishes reachability.

### Extraction result — confirmed

Both report `header_valid=1`, with `module_address` `0x15e5` for `ez2dj1st` and `0x4c5c` for `ez2dj5th`, and non-zero `id_ref` and `id_verify` identical across requests. The addresses show the family: 1st sits in the same band as 1st SE (`0x15e1`) and 5th in the same band as 6th (`0x4c51`).

**No raw ID is recorded in the repository.** They live only in the `[ez2dj1st]` and `[ez2dj5th]` sections of `cfg/hardlock-id.ini`, with the existing `[ez2dj6th]` and `[ez2dj1stse]` sections preserved.

### Candidate judgement — confirmed

Both products resolved to exactly one candidate: `candidate-84` of 134 for `ez2dj1st`, separated by seventeen descriptor requests against sixteen and 275 trace lines against 245 or 247; and `candidate-135` of 176 for `ez2dj5th`, separated by forty descriptor requests against thirty-eight and 363 lines against 329 or 331.

On the first pass even the winning candidate opened no asset and stopped at `0xc0000096`, a privileged instruction — untrapped port I/O, because both profiles carried helper RVAs inherited from a different build. Decoding the code window at each fault gave each build's own pair: `0x00035757` and `0x0003577b` for `ez2dj1st`, replacing the 1st SE `.gtide` values `0x00038987` and `0x000389ab`; and `0x000ca067` and `0x000ca09b` for `ez2dj5th`, replacing 4th's `0x000c3817` and `0x000c384b`. The runtime compares the exception address, so the values it wants are the `in`/`out` opcode bytes and not the helper entry points.

Re-run with the corrected RVAs, both candidates make the guest read its own assets: `ez2dj1st` goes from 275 trace lines and no asset opens to 1875 lines and 484 opens across 248 distinct paths, and `ez2dj5th` from 363 lines and none to 2493 lines and 67 opens. A wrong candidate cannot invent the real asset paths, so both maps are confirmed.

The `response450` and `tail44c` used by 3rd, 4th and 1st SE hold for both: **five products share one handshake replay.**

### Code change

The `ez2dj1st` built-in profile turns `hle_windows_directory` off and `hle_dynamic_vfs` on, moves the device prefix from `\\.\LPTDI` to `\\.\FEnteDev`, clears the LPTDI target-state hex, turns on `hardlock_cfg_material_default`, and takes the corrected helper RVAs.

The `ez2dj5th` profile keeps the 4th compatibility baseline but gains its own helper RVAs and a note matching what was observed; its `MakeChdCompatibilityProfile` call became a block like 6th's to allow the override.

The CHD compatibility check in `tests/unit/target_profile_test.cpp` now takes the expected helper RVA pair as arguments, because the helpers sit at a different place in each build and cannot share one baseline.

### Placed material

Only under the Git-ignored `cfg/`: the confirmed challenge-response maps as `cfg/hardlock-ez2dj1st.map` and `cfg/hardlock-ez2dj5th.map`, the `response450` and `tail44c` under `[ez2dj1st]` and `[ez2dj5th]` in `cfg/hardlock.ini`, and the three descriptor values in `cfg/hardlock-id.ini`.

### Verification

The full Windows x86 Release build succeeded, `re2dj_unit_tests.exe` reported `checks: 1424, failures: 0`, and the product-loader probe reported all four items ok. `re2dj ez2dj1st` runs with no extra options: every preparation item true, `iat_verified=true`, outcome `success`, all three `hardlock_cfg_material` items recognised, fifteen transforms and 484 asset opens. `ez2dj5th` recognises the same three items from the profile defaults alone, with thirty-seven transforms and 71 asset opens.

### Not verified

The `re2dj ez2dj5th` product path fails with `CHD MBR has no in-range FAT32 partition`. The supplied `ez2dj5.chd` is a whole-disk FAT32 volume with no partition table — LBA 0 is itself the FAT32 boot sector — which the current reader does not recognise. Every 5th observation here used the extracted directory as the source root and ran the real 5th executable.

### Remaining work

- ~~Support for a whole-disk FAT32 volume with no partition table~~ — done in [task 238](20260910-238-partitionless-fat32-volume.md); `ez2dj5th` now runs from its CHD.
- Confirming what either product displays. This task covered the Hardlock and I/O boundaries only.
- Obtaining a real dongle `response450`; the current value is the synthetic oracle established on 3rd.
