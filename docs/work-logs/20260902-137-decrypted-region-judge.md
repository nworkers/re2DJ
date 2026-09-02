# 복호화 영역 판별기 작업 로그

관련 설계: [복호화 영역 판별기](../design/20260902-137-decrypted-region-judge.md), 관련 작업 지시: [작업 지시](../work-orders/20260902-137-decrypted-region-judge.md)

*Related design: [decrypted region judge](../design/20260902-137-decrypted-region-judge.md); related work order: [work order](../work-orders/20260902-137-decrypted-region-judge.md).*

## 결과

- 플랫폼 중립 `re2dj::analysis::ScoreCodeRegion`을 추가했습니다. 바이트 span 하나에서 Shannon 엔트로피, `55 8b ec` prologue 수, 길이 4 이상 `cc` run 수, zero byte 비율을 계산하고 `ciphertext-like` / `code-like` / `indeterminate` 3상태로 보고합니다.
- 청크 분할 함수 `ScoreCodeRegionChunks`를 추가했습니다. 기본 청크는 transform challenge 단위와 같은 `0x8000`입니다.
- 오프라인 도구 `re2dj_code_score`를 추가했습니다. 파일, `--hdd` 디렉터리, `--chd` 이미지 중 하나에서 바이트를 얻어 PE면 섹션 단위로, 아니면 바이트 범위로 점수를 냅니다.
- `--require-code`가 `code-like` 구간이 없을 때 종료 코드 `3`을 돌려줍니다. 읽기 실패(`2`)와 구분되므로 후보 수십 개를 shell loop로 거를 수 있습니다.
- unit test 26개 검사를 추가했습니다. 합성 데이터만 쓰며 원본 자산에 의존하지 않습니다.

*Added the platform-neutral `re2dj::analysis::ScoreCodeRegion`, which computes Shannon entropy, `55 8b ec` prologue count, `cc` runs of four or more, and zero-byte share from one byte span and reports `ciphertext-like`, `code-like`, or `indeterminate`; added `ScoreCodeRegionChunks` with a `0x8000` default matching the transform challenge granularity; added the offline `re2dj_code_score`, which takes bytes from a file, an `--hdd` directory, or a `--chd` image and scores by PE section or by byte range; gave `--require-code` exit code `3`, distinct from the read failure `2`, so dozens of candidates can be filtered from a shell loop; and added twenty-six unit-test checks using synthetic data only, with no dependency on original assets.*

## 지표 정의에서 바로잡은 것

`cc` padding을 **바이트 수가 아니라 run 수**로 정의했습니다. 균등 난수 901,120바이트에는 `0xcc` 바이트가 약 3,520개 나타나므로, 바이트 수로 세면 분석 문서가 기록한 "`cc` padding 0회"를 재현할 수 없습니다. 길이 4 이상 연속은 균등 난수에서 위치당 확률이 `2^-32`이므로 기대값이 사실상 0이고, 이것이 기록된 관측과 일치합니다. 실제 측정에서도 원본 여섯 섹션 전부 run 수 0으로 나왔습니다.

*Corrected in the metric definitions: `cc` padding is counted as **runs, not bytes**. Uniform random data of 901,120 bytes holds about 3,520 `0xcc` bytes, so a byte count cannot reproduce the analysis document's "zero `cc` padding", while a run of four or more has probability `2^-32` per position and an expectation near zero — which matches the recorded observation, and the real measurement returned zero runs for all six sections.*

## 검증 — 기록값 재현

원본 CHD의 `EZ2DJ.EXE`를 직접 읽어 섹션 점수를 냈습니다. 자산은 추출하지 않았습니다.

```
re2dj_code_score --chd roms/ez2dj4th/4thTrax.chd --guest-path EZ2DJ/EZ2DJ.EXE --chunk 0
```

| section | 측정 엔트로피 | 기록값 | prologue | `cc` run | 판정 |
| --- | --- | --- | --- | --- | --- |
| `.text` | `7.9967` | `7.9967` | 0 | 0 | ciphertext-like |
| `.rdata` | `7.8962` | `7.896` | 0 | 0 | indeterminate |
| `.data` | `7.9975` | `7.998` | 0 | 0 | ciphertext-like |
| `.idata` | `3.4734` | — | 0 | 0 | indeterminate |
| `.reloc` | `7.7558` | — | 0 | 0 | indeterminate |
| `.protect` | `7.9690` | `7.969` | 3 | 0 | indeterminate |

**확인됨.** 기록된 네 값이 모두 재현되었으므로 이전 임시 계산과 이번 측정기가 서로를 검증합니다.

*Verification — reproducing the record. Scoring `EZ2DJ.EXE` read directly from the original CHD, with no asset extracted, reproduced all four recorded figures, so the earlier ad hoc calculation and this measurement tool validate each other. **Confirmed.***

## 새로 확인된 사실 — `.protect` 머리 1 KiB는 평문 코드다

섹션 판정에서 `.protect`만 prologue가 0이 아니었습니다. 청크를 좁혀 위치를 특정했습니다.

| 구간 | 엔트로피 | prologue | zero% | 판정 |
| --- | --- | --- | --- | --- |
| `.protect +0x000000` (1 KiB) | `2.8456` | 3 | `65.7%` | **code-like** |
| `.protect +0x000400` 이후 | `7.3` ~ `7.98` | 0 | `0.1%` ~ `11%` | indeterminate / ciphertext-like |

섹션 전체에서 관찰된 prologue 3회가 전부 첫 1 KiB에 있습니다. 균등 난수 237,568바이트의 기대값은 약 `0.014`회이므로 우연으로 보기 어렵습니다.

이 결과는 두 가지를 줍니다. 첫째, "원본 이미지는 전 구간 암호문"이라는 서술에 예외가 하나 있음이 확인되었습니다. `.text`에 평문 구간이 없다는 결론은 그대로입니다. 둘째, **판별기가 이 실행 파일 안의 실제 평문 코드를 실제로 집어냈습니다.** 합성 데이터가 아니라 목표 바이너리 자체에서 얻은 positive control입니다.

`.protect` 머리가 보호의 최초 실행 stub이라는 해석은 **추정**입니다. 진입점 RVA `0x006e0240`이 `.text` 밖이라는 기존 관찰과 일관되지만, 이 1 KiB가 진입점을 포함하는지는 확인하지 않았습니다. 분석 문서에 그렇게 표기했습니다.

*A newly confirmed fact — the first 1 KiB of `.protect` is plaintext code. `.protect` was the only section with a non-zero prologue count, and narrowing the chunk located it: the first 1,024 bytes read entropy `2.8456` with three prologues and `65.7%` zero bytes and are judged **code-like**, while everything from `+0x400` onward carries no prologue. All three prologues observed in the section are in that 1 KiB, against an expectation of about `0.014` for 237,568 uniform random bytes. This gives two things: the statement "the original image is ciphertext throughout" now has one confirmed exception, with the conclusion that `.text` holds no plaintext region unchanged; and **the judge found real plaintext code inside the target binary itself**, a positive control from the actual image rather than synthetic data. Reading the `.protect` head as the protection's first executing stub is **inferred** — consistent with the existing observation that entry RVA `0x006e0240` lies outside `.text`, but whether this 1 KiB contains the entry point was not verified, and the analysis document marks it that way.*

## 검증 — 빌드와 테스트

- `cmake --preset windows-x86-debug` 구성 성공
- `re2dj_code_score`와 `re2dj_unit_tests` 빌드 성공, 경고 없음
- unit test `checks: 1193, failures: 0`
- 원본 자산 없이 빌드되는 구조를 유지했습니다. 새 테스트는 합성 데이터만 씁니다.

*Verification — build and tests: the Windows x86 preset configured, `re2dj_code_score` and `re2dj_unit_tests` built without warnings, and the suite reported `checks: 1193, failures: 0`. The build still requires no original assets, and the new tests use synthetic data only.*

## 다음 단계

1. launcher probe가 fault 시점의 게스트 이미지 구간을 읽어 같은 판별기에 넣도록 통합합니다. 현재는 실행 중 memory dump 경로가 없어 Stage 7 자동화가 절반만 되어 있습니다.
2. `.protect` 머리 stub이 진입점을 포함하는지 확인합니다. 섹션 vaddr과 entry RVA `0x006e0240`을 대조하면 됩니다.
3. reSoftlock이 `seeds`와 `map-batch`를 구현하면 Stage 5 사전 선별에 이 도구를 그대로 씁니다.

*Next: integrate the launcher probe so it reads the guest image region at fault time and feeds the same judge, since without an in-run memory dump path Stage 7 is only half automated; check whether the `.protect` head stub contains the entry point by comparing section virtual addresses against entry RVA `0x006e0240`; and use this tool as-is for the Stage 5 pre-filter once reSoftlock implements `seeds` and `map-batch`.*
