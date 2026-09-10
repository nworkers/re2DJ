# 작업 로그: ez2d2m Hardlock descriptor 확보

## 한국어

### 관련 문서

- 절차: [Hardlock descriptor ID 추출](../guides/hardlock-descriptor-extraction.md)
- 분석: [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)의 2026-09-10 절
- 선행 작업: [ez2d2m target 추가와 CHD 재귀 추출](20260910-239-ez2d2m-target-and-chd-extract.md), [ez2dj1st·ez2dj5th Hardlock descriptor 확보](20260909-237-ez2dj1st-5th-hardlock-descriptors.md)

### 요구사항

`ez2d2m`의 `id_ref`, `id_verify`, `module_address`를 추출합니다.

이 작업은 코드를 바꾸지 않았습니다. 기존 도구와 절차를 쓴 관측 작업이므로 작업 지시서 없이 로그와 분석 갱신만 남깁니다.

### 결과 — 확인됨

descriptor 12건이 모두 `header_valid=1`, 256바이트로 관측됐고 값이 요청 전체에서 하나로 일치합니다.

| 항목 | 값 |
| --- | --- |
| `module_address` | `0x4c5e` |
| `id_ref` | non-zero, 요청 전체 동일 |
| `id_verify` | non-zero, 요청 전체 동일, `id_ref`와 다른 값 |

원문 ID는 `cfg/hardlock-id.ini`의 `[ez2d2m]` section에만 있습니다. `cfg/`는 Git이 무시하므로 저장소에는 들어가지 않습니다. 세부 관측은 분석 문서에 있습니다.

증거: `logs/windows_x86_launcher_probe/ez2d2m/20260910-133835-124.jsonl`과 같은 이름의 `.vfs.log` 227줄.

### 도중에 나온 것

#### 회귀로 오인한 셸 인용 문제

처음 세 번의 실행에서 `\\.\FEnteDev` 개방이 `error=123`으로 실패했습니다. 같은 명령을 **알려진 정상 프로파일 `ez2dj3rd`** 로 돌려도 똑같이 실패했고, 2026-09-02의 3rd 로그는 같은 지점에서 `success=1`에 322줄이었습니다. 이 시점에서는 device mock 회귀로 보였습니다.

runtime에 임시 계측을 넣어 실제 값을 찍어 보니 원인이 달랐습니다.

```
prefix=[\.\FEnteDev]:name=[\\.\FEnteDev]:match=0
```

`--device-mock-lptdi-path-prefix`에 넘긴 백슬래시 하나가 Git Bash를 거치며 사라진 것이었습니다. 인용을 바꿔도(큰따옴표, 작은따옴표) 결과는 같았습니다. **회귀가 아니었습니다.**

옵션을 생략해 프로파일의 `device_mock_path_prefix`를 쓰게 하니 `match=1`, `success=1`로 바로 통과했습니다. 이 함정과 확인 방법을 절차 문서에 추가했습니다.

교훈은 진단 대상을 좁힐 때 적용됩니다. 알려진 정상 프로파일에서도 같은 증상이 나왔다는 사실은 "코드 회귀"와 "명령 문제"를 구분하지 못합니다. 두 실행이 같은 셸을 공유했기 때문입니다. 계측 한 줄이 그 구분을 즉시 지었습니다.

#### 절차 문서의 공백

`.protect` 계열은 문서화된 명령만으로 descriptor에 도달하지 못합니다. `--hle-dynamic-vfs`, `--run-detached`, handshake 응답 세 가지가 더 필요하며, 앞 둘이 없을 때의 증상은 서로 다릅니다. 작업 237이 앞 둘을 로그에 적었지만 절차 문서에는 반영되지 않았습니다. 이번에 세 항목과 각각의 증상을 절차 문서에 넣었습니다.

또한 `cfg/hardlock.ini`의 프로파일 section은 같은 이름의 transform map이 있을 때만 적용됩니다. 새 프로파일에는 map이 없으므로 재생값을 명시적 옵션으로 넘겨야 합니다. 이것도 문서에 넣었습니다.

#### handshake 재생값의 범위

3rd·4th·1st SE·1st·5th에서 쓰던 재생값이 `ez2d2m`에서도 그대로 성립합니다. EZ2DJ가 아닌 제품까지 포함해 **여섯 제품이 같은 handshake 재생값을 씁니다.**

### 검증

| 항목 | 결과 |
| --- | --- |
| descriptor 요청 | 12건, 전부 `header_valid=1`, 256바이트 |
| `module_address` 일관성 | 12건 전부 `0x4c5e` |
| `id_ref`·`id_verify` 일관성 | 각각 요청 전체에서 단일 해시 |
| dump 파일 | `cfg/hardlock-id.ini`에 `[ez2d2m]` section 생성 |
| 임시 계측 제거 | `git diff src/platform/windows/injected_runtime.cpp` 비어 있음, 재빌드 완료 |

### 남은 것

- **유효한 transform 응답 map.** transform 요청 11건에 답할 수 없어 게스트는 자기 자산을 하나도 열지 못했고 `0xc0000005`로 끝납니다. descriptor 도달은 보호 통과가 아닙니다.
- `module_address` 대역의 의미. `0x4c5e`가 5th·6th와 같은 대역인 것은 관측이지 설명이 아닙니다.
- 실제 dongle 재생값. 현재 값은 3rd에서 확인된 synthetic oracle입니다.

## English

### Related documents

- Procedure: [Hardlock descriptor ID extraction](../guides/hardlock-descriptor-extraction.md)
- Analysis: the 2026-09-10 section of [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md)
- Preceding tasks: [the ez2d2m target and recursive CHD extraction](20260910-239-ez2d2m-target-and-chd-extract.md), [ez2dj1st and ez2dj5th Hardlock descriptors](20260909-237-ez2dj1st-5th-hardlock-descriptors.md)

### Requirement

Extract `id_ref`, `id_verify` and `module_address` for `ez2d2m`.

This task changed no code. It is an observation using existing tools and the existing procedure, so it leaves a log and an analysis update rather than a work order.

### Result — confirmed

Twelve descriptors were observed, all 256 bytes with `header_valid=1`, and their values agree across every request: `module_address` is `0x4c5e`; `id_ref` and `id_verify` are both non-zero and constant, and differ from each other.

The raw IDs exist only in the `[ez2d2m]` section of `cfg/hardlock-id.ini`. All of `cfg/` is Git-ignored, so they do not enter the repository. The detailed observation is in the analysis document.

Evidence: `logs/windows_x86_launcher_probe/ez2d2m/20260910-133835-124.jsonl` and its 227-line `.vfs.log`.

### What came up along the way

#### A shell-quoting problem mistaken for a regression

The first three runs failed to open `\\.\FEnteDev`, reporting `error=123`. Running the same command against **`ez2dj3rd`, a known-good profile**, failed identically, while the 2026-09-02 log for 3rd showed `success=1` at that point and a 322-line trace. At that stage this looked like a device-mock regression.

Temporary instrumentation in the runtime printed the actual values and showed otherwise:

```
prefix=[\.\FEnteDev]:name=[\\.\FEnteDev]:match=0
```

One backslash of the `--device-mock-lptdi-path-prefix` value was lost passing through Git Bash, under either quoting style. **It was not a regression.**

Omitting the option so the profile's own `device_mock_path_prefix` is used gave `match=1` and `success=1` immediately. The trap and the way to check for it are now in the procedure document.

The lesson applies to narrowing a diagnosis. Reproducing the same symptom on a known-good profile did not distinguish "code regression" from "bad command", because both runs shared the same shell. One line of instrumentation settled it at once.

#### Gaps in the procedure document

The `.protect` family does not reach a descriptor from the documented command alone. Three additions are needed — `--hle-dynamic-vfs`, `--run-detached` and a handshake response — and the first two fail with different symptoms. Task 237 recorded the first two in its log but they never reached the procedure. All three, with their symptoms, are now in the procedure.

A `cfg/hardlock.ini` profile section is also applied only alongside a transform map of the same name. A new profile has no map, so the replay values must be passed as explicit options. That is now documented too.

#### The reach of the handshake replay

The replay values used for 3rd, 4th, 1st SE, 1st and 5th hold for `ez2d2m` as well. Including a product that is not EZ2DJ, **six products now share one handshake replay.**

### Verification

| Item | Result |
| --- | --- |
| Descriptor requests | 12, all `header_valid=1` at 256 bytes |
| `module_address` consistency | `0x4c5e` in all 12 |
| `id_ref` / `id_verify` consistency | a single hash each across every request |
| Dump file | `[ez2d2m]` section created in `cfg/hardlock-id.ini` |
| Instrumentation removed | `git diff src/platform/windows/injected_runtime.cpp` is empty and the runtime was rebuilt |

### What remains

- **A valid transform response map.** With no answer for the eleven transform requests the guest opened none of its own assets and ended at `0xc0000005`. Reaching the descriptor is not passing the protection.
- What the `module_address` band means. That `0x4c5e` shares a band with 5th and 6th is an observation, not an explanation.
- Real dongle replay values. The current ones are the synthetic oracle confirmed on 3rd.
