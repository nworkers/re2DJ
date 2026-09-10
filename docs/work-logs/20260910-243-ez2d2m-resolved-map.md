# 작업 로그: ez2d2m 확정 map 배치와 main 머지

## 한국어

### 관련 문서

- 작업 지시: [ez2d2m 확정 map 배치와 main 머지](../work-orders/20260910-243-ez2d2m-resolved-map.md)
- 분석: [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)의 `candidate-70` 확정 절
- 선행 작업: [런타임 challenge map 재판별](20260910-242-ez2d2m-runtime-map-judgement.md), [후보 map 검증과 판별](20260910-241-ez2d2m-candidate-judgement.md), [Hardlock descriptor 확보](20260910-240-ez2d2m-hardlock-descriptor.md), [target 추가와 CHD 재귀 추출](20260910-239-ez2d2m-target-and-chd-extract.md)

### 요구사항

`candidate-70`을 정답으로 확정하고 `main`에 머지합니다.

### 한 일

확정 자료를 다른 제품과 같은 형태로 배치했습니다. `cfg/hardlock-ez2d2m.map`에 11행 map, `cfg/hardlock.ini`의 `[ez2d2m]` section에 `response450`과 `tail44c`입니다. `cfg/`는 Git이 무시하므로 저장소에는 들어가지 않습니다.

`ez2d2m` 프로파일의 `note`를 관측에 맞게 정정했습니다. 이전 문구는 descriptor와 port-helper 주소와 실행 결과가 모두 미확인이라고 적고 있었는데, 세 가지 모두 이제 관측됐습니다. 정정한 문구는 `module_address 0x4c5e`, 보호 통과, 원본 `.text` RVA `0x0000b565`의 정지 지점을 적고, 그 주소를 byte 폭 helper 필드에 넣으면 안 된다는 점을 함께 남깁니다.

### 검증 — 확인됨

Hardlock 옵션을 하나도 주지 않은 실행에서 launcher가 `hardlock_cfg_material`의 세 항목을 모두 인식하고 map을 `entries=11`로 로드했습니다. 결과는 명시적 옵션을 준 실행과 같습니다.

| 항목 | 값 |
| --- | --- |
| `hardlock_cfg_material` | `response450` `tail44c` `map` 모두 true |
| transform map | `entries=11` |
| trace | 248줄, handshake 4회, descriptor 14회 |
| transform 매핑 | 11건 전부 `mapped=1`, `unmapped=0` |
| 정지 지점 | 원본 `.text` RVA `0x0000b565`, `0xc0000096`, `edx=0x030a` |

| 항목 | 결과 |
| --- | --- |
| Windows x86 Debug 전체 build | 통과, 경고 0건 |
| CTest (`re2dj_windows_vfs_runtime_probe` 제외) | 3/3 통과 |
| 단위 시험 | checks 1481, failures 0 |

`re2dj_windows_vfs_runtime_probe`는 작업 239부터 계속 멈춥니다. 윈도우 생성 단계에서 멈추며 이 작업의 변경과 무관합니다.

### 확정의 한계

이 저장소가 다른 제품에서 써 온 최종 기준은 게스트가 자기 자산을 읽는 것이고, `ez2d2m`은 아직 `asset-open` 0건입니다. 게스트가 자산 로딩 전에 캐비닛 출력에서 멈추기 때문이며, 16비트 I/O 경계가 없으면 그 기준을 적용할 수 없습니다. 이 확정은 판별 근거에 따른 소유자의 판단이며 자산 로딩으로 검증된 것은 아닙니다. 근거와 한계는 분석 문서에 남겼습니다.

### 남은 것

- 16비트 폭 legacy I/O bus와 `0x300` 대역. `ez2d2m`의 유일한 차단 지점입니다.
- 그 뒤 자산 로딩으로 `candidate-70`을 재검증.
- 입력 helper 위치와 나머지 port의 bit 배치.
- `re2dj_windows_vfs_runtime_probe` 정지 원인.

## English

### Related documents

- Work order: [placing the ez2d2m resolved map and merging to main](../work-orders/20260910-243-ez2d2m-resolved-map.md)
- Analysis: the `candidate-70` adoption section of [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md)
- Preceding tasks: [runtime-challenge map re-judgement](20260910-242-ez2d2m-runtime-map-judgement.md), [candidate artifact verification and judgement](20260910-241-ez2d2m-candidate-judgement.md), [Hardlock descriptor extraction](20260910-240-ez2d2m-hardlock-descriptor.md), [target addition and recursive CHD extraction](20260910-239-ez2d2m-target-and-chd-extract.md)

### Requirement

Adopt `candidate-70` as resolved and merge to `main`.

### What was done

The resolved material was placed in the same shape as for the other products: the eleven-row map at `cfg/hardlock-ez2d2m.map`, and `response450` and `tail44c` in the `[ez2d2m]` section of `cfg/hardlock.ini`. `cfg/` is Git-ignored, so none of it enters the repository.

The `ez2d2m` profile `note` was corrected against observation. It previously said the descriptor, the port-helper addresses and execution success were all unconfirmed; all three have now been observed. The corrected text records `module_address 0x4c5e`, that it passes the protection, and the stop at RVA `0x0000b565` in the original `.text`, along with the warning that this address must not go into the byte-width helper fields.

### Verification — confirmed

Run with no Hardlock option at all, the launcher recognised all three `hardlock_cfg_material` items and loaded the map as `entries=11`, giving the same result as the run with explicit options: 248 trace lines, 4 handshakes, 14 descriptors, all 11 transforms mapped with `unmapped=0`, and the stop at `0xc0000096`, RVA `0x0000b565`, `edx=0x030a` in the original `.text`.

The full Windows x86 Debug build passed with zero warnings, CTest passed 3/3 excluding `re2dj_windows_vfs_runtime_probe`, and the unit tests report 1481 checks with 0 failures. That probe has hung since task 239, in its window-creation step, unrelated to these changes.

### The limit of this adoption

The final bar this repository has used for other products is the guest reading its own assets, and `ez2d2m` still shows zero `asset-open`, because the guest stops at cabinet output before loading anything. That bar cannot be applied without a 16-bit I/O boundary. This adoption is the owner's judgement on the judgement evidence, not a result validated by asset loading; the evidence and the limit are recorded in the analysis document.

### What remains

A 16-bit-wide legacy I/O bus over the `0x300` band, which is the single thing blocking `ez2d2m`; re-validating `candidate-70` through asset loading afterwards; the input helper's location and the bit layout of the remaining ports; and the cause of the `re2dj_windows_vfs_runtime_probe` hang.
