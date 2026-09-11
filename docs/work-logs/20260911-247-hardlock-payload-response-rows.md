# 작업 로그: Hardlock 요청 단위 응답 행

## 한국어

### 관련 문서

- 설계: [Hardlock 요청 단위 응답 행](../design/20260911-247-hardlock-payload-response-rows.md)
- 작업 지시: [Hardlock 요청 단위 응답 행](../work-orders/20260911-247-hardlock-payload-response-rows.md)
- 분석: [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)의 2026-09-11 절, [ez2dj6th Hardlock 분석](../analysis/ez2dj6th-hardlock.md)의 2026-09-11 추정
- 지식: [Hardlock API function 코드](../kb/hardlock-api-functions.md)
- 계약: [reSoftlock 인터페이스 계약](../design/20260902-136-resoftlock-interface-contract.md) 1.1절
- 선행: [CHD 내부 디렉터리를 프로파일에서 받기](20260910-246-chd-root-from-profile.md)

### 요구사항

`ez2d2m` 작업을 이어갑니다. 작업 246이 남긴 차단 지점은 응답할 수 없는 12번째 transform(`function=0x0011`, 7블록)입니다.

### 작업 환경

작업 239–246은 다른 머신(`C:\workspace\git\re2DJ`)에서 진행됐습니다. 이 머신(`E:\MYWORK\Projects\re2DJ`)에는 그 실행 로그와 `cfg/ez2d2m/runtime-challenges-full.txt`가 없었습니다. 사용자가 작업 중 reSoftlock을 갱신해 그 관찰이 reSoftlock `artifacts/ez2d2m/runtime-challenges-full.txt`로 들어왔고, 이 작업의 대조에 썼습니다. 빌드는 이 머신에서 새로 구성했습니다.

### 한 일과 발견

**1. 기준 재현.** `re2dj ez2d2m`이 이 머신에서도 trace 261줄, 264바이트 transform 11건 전부 매핑, 312바이트 transform `unmapped=7`, `ExitProcess(0)`으로 끝났습니다(`20260911-001701-194`).

**2. 7블록 입력은 결정적이지 않습니다.** `--hardlock-transform-input-dump`로 두 번 받아(`001811-508`, `001814-961`) 이전 머신 관찰과 대조했습니다. 11개 `0x000e` challenge는 세 관찰이 모두 같습니다. `0x0011`은 **블록 5만** 세 관찰이 같고, 블록 0·1·6은 두 실행의 runtime 적재 주소 차이(`0x50000`)만큼 정확히 움직이며, 블록 2–4는 이 머신 안에서는 같지만 이전 머신과 다릅니다. 바이트는 어디에도 기록하지 않았습니다.

**3. 상위 계약 대조.** 작업 130과 같은 방식으로 2EZConfig-V2(GPL-3.0-or-later)를 사실 대조에만 썼습니다. `fastapi.h`는 `API_CRYPT = 14`, `API_CODE = 17`을 정의하고, 에뮬레이터의 `API_CODE`는 끝에서 두 번째 블록으로 한 번 계산해 payload 여러 곳에 쓰고 블록 3에는 더하며 마지막 블록은 유지합니다. 계산 입력 위치가 정확히 변하지 않는 블록 5입니다. 코드는 옮기지 않았습니다.

**4. 결론.** 현재 map의 블록 행으로는 이 응답을 담을 수 없고, 요청 전체를 정확히 비교하는 행도 다음 실행에서 빗나갑니다. 그래서 `??`를 허용하는 **요청 행**을 설계하고 구현했습니다.

### 코드 변경

| 파일 | 변경 |
| --- | --- |
| `include/re2dj/hle/hardlock/payload_responses.h`, `src/hle/hardlock/payload_responses.cpp` | 새 모듈. 요청 행 자료형, `??` 패턴 파싱, 모호성 판정, 일치·적용, 고정 폭 record pack/unpack |
| `transform_responses.{h,cpp}` | `HardlockTransformResponseMap`과 `ParseHardlockTransformResponseMap`(옛 `ParseHardlockTransformResponseTable` 대체), 행 길이로 종류 분기, `PackHardlockTransformResponseMap` |
| `protocol.h` | `kHardlockTransformBlockSize`를 프로토콜 상수로 이동 |
| `device.{h,cpp}` | 요청 행 우선 적용, `transform_payload_mapped` |
| `injected_runtime.cpp` | export `g_re2dj_hardlock_payload_response_count`·`g_re2dj_hardlock_payload_responses`, unpack, trace `payload=` 필드. 블록 행 배열 크기를 상수로 표현(4096바이트 그대로) |
| launcher `main.cpp`, `child_process_handoff.{h,cpp}` | 공용 pack 사용, 두 종류 행 전달, **용량 초과 map 거절**, JSONL `payload_entries` |
| 단위 시험 | `hardlock_payload_responses_test.cpp` 추가, map 파서·장치 시험 확장 |

용량 검사는 기존 결함도 막습니다. 이전에는 256행을 넘는 map이 runtime 배열 밖으로 쓰일 수 있었습니다.

### 검증

| 항목 | 결과 |
| --- | --- |
| Windows x86 Debug 전체 build | 통과, 경고 0건 |
| 단위 시험 | checks 1635, failures 0 (이전 1550) |
| `ez2d2m` 기본 실행 (`003542-469`, 최종 빌드 `004117-727`) | 261줄, `entries=11`, `payload_entries=0`, 12번째 `unmapped=7:payload=0` — 변경 전과 동일 |
| 항등 요청 행 smoke (`003607-026`) | `payload_entries=1`, 적재 주소가 다른 새 실행에서 12번째 `mapped=0:unmapped=0:payload=1`, 게스트 동작 동일(261줄) |
| `ez2dj3rd` (전 `002454-923`, 후 `003624-597`) | 313줄, asset-open 0, crash 1, transform 32건으로 동일 |

항등 요청 행은 블록 5만 지정하고 그 값을 되돌려 쓰는 행입니다. 새 보호 응답이 아니며 게스트가 받는 바이트는 echo와 같습니다. 이 행과 입력 dump는 Git이 무시하는 `cfg/ez2d2m/`에만 있습니다(`identity-request-row.map`, `runtime-transform-inputs-run1.txt`, `runtime-transform-inputs-run2.txt`).

### 남은 것

- **유효한 `API_CODE` 요청 행.** 외부 도구의 몫입니다. 현재 reSoftlock은 `0x0011`에 블록별 `HL_CRYPT`를 쓰는 가설 경로만 있어 모양이 맞지 않습니다. 필요한 것은 블록 5(와 블록 3)를 입력으로 받아 요청 행 하나를 내보내는 경로이며, 변하는 블록은 입력에서 `??`로 둬야 합니다.
- 6th의 7개 입력이 같은 구조인지. 재판별 전에 여러 번 받아 확인해야 합니다.
- 관찰만 하고 다루지 않은 것: `ez2dj3rd`의 transform 32건이 이 머신에서 모두 `unmapped=1`입니다. 이 작업 전 기준 실행에서도 같았으므로 이 변경의 회귀는 아니지만, 원인은 확인하지 않았습니다.
- `ez2d2m` 프로파일의 `note` 문자열이 작업 244 이전 상태("raw I/O is disabled", `out dx, ax`에서 정지)를 설명하고 있습니다. 이 작업 범위 밖이라 고치지 않았습니다.

## English

### Related documents

- Design: [Hardlock request-level response rows](../design/20260911-247-hardlock-payload-response-rows.md)
- Work order: [Hardlock request-level response rows](../work-orders/20260911-247-hardlock-payload-response-rows.md)
- Analysis: the 2026-09-11 section of [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md) and the 2026-09-11 inference in [the ez2dj6th Hardlock analysis](../analysis/ez2dj6th-hardlock.md)
- Knowledge base: [Hardlock API function codes](../kb/hardlock-api-functions.md)
- Contract: section 1.1 of [the reSoftlock interface contract](../design/20260902-136-resoftlock-interface-contract.md)
- Preceding: [taking the image-internal directory from the profile](20260910-246-chd-root-from-profile.md)

### Requirement

Continue the `ez2d2m` work. The blocker task 246 left is the unanswerable twelfth transform, `function=0x0011` with seven blocks.

### Working environment

Tasks 239 to 246 ran on another machine (`C:\workspace\git\re2DJ`), so this machine (`E:\MYWORK\Projects\re2DJ`) had neither their run logs nor `cfg/ez2d2m/runtime-challenges-full.txt`. During this task the user updated reSoftlock, which brought that capture in as reSoftlock `artifacts/ez2d2m/runtime-challenges-full.txt`, and it was used for comparison here. The build was configured afresh on this machine.

### What was done and found

**1. Baseline reproduced.** `re2dj ez2d2m` ends on this machine exactly as before: 261 trace lines, all eleven 264-byte transforms mapped, the 312-byte transform at `unmapped=7`, then `ExitProcess(0)` (`20260911-001701-194`).

**2. The seven-block input is not deterministic.** It was captured twice with `--hardlock-transform-input-dump` (`001811-508`, `001814-961`) and compared with the other machine's capture. The eleven `0x000e` challenges are identical in all three. In `0x0011` **only block 5** is identical in all three; blocks 0, 1 and 6 move by exactly the runs' runtime load-address difference (`0x50000`), and blocks 2 to 4 match within this machine but not the other. No byte was recorded anywhere.

**3. Upstream contract comparison.** As in task 130, 2EZConfig-V2 (GPL-3.0-or-later) was used for fact comparison only. Its `fastapi.h` defines `API_CRYPT = 14` and `API_CODE = 17`, and its emulator's `API_CODE` computes once from the second-to-last block, writes several places in the payload, adds into block 3 and keeps the last block. The input position is exactly block 5, the one that never changes. No code was carried over.

**4. Conclusion.** The current map's block rows cannot hold this answer, and a row comparing the whole request exactly would miss on the next run, so **request rows** that allow `??` were designed and implemented.

### Code changes

The new `payload_responses` module (`include/re2dj/hle/hardlock/payload_responses.h`, `src/hle/hardlock/payload_responses.cpp`) owns the request-row type, `??` pattern parsing, the ambiguity test, matching and applying, and the fixed-width record pack/unpack. `transform_responses.{h,cpp}` gained `HardlockTransformResponseMap` and `ParseHardlockTransformResponseMap`, replacing `ParseHardlockTransformResponseTable`, dispatching on row length, plus `PackHardlockTransformResponseMap`. `protocol.h` now owns `kHardlockTransformBlockSize`. `device.{h,cpp}` apply request rows first and report `transform_payload_mapped`. `injected_runtime.cpp` gained the `g_re2dj_hardlock_payload_response_count` and `g_re2dj_hardlock_payload_responses` exports, the unpacking and the trace's `payload=` field, and now sizes the block-row array from constants, still 4096 bytes. The launcher's `main.cpp` and `child_process_handoff.{h,cpp}` use the shared packing, transfer both row kinds, **refuse a map beyond capacity**, and log `payload_entries`. Unit tests gained `hardlock_payload_responses_test.cpp` and extended the map-parser and device tests.

The capacity check also closes an existing defect: a map of more than 256 rows could previously be written past the runtime array.

### Verification

The full Windows x86 Debug build passes with zero warnings, and unit tests report 1635 checks with 0 failures, up from 1550. The `ez2d2m` default run (`003542-469`, and `004117-727` on the final build) is unchanged: 261 lines, `entries=11`, `payload_entries=0`, and a twelfth transform at `unmapped=7:payload=0`. The identity request-row smoke test (`003607-026`) reports `payload_entries=1`, and on a fresh run with a different load address the twelfth transform reports `mapped=0:unmapped=0:payload=1` while the guest behaves as before at 261 lines. `ez2dj3rd` is unchanged before (`002454-923`) and after (`003624-597`): 313 lines, zero asset opens, one crash, 32 transforms.

The identity request row specifies only block 5 and writes it back. It is not a new protection answer — the guest receives the same bytes as the echo — and it lives, with the input dumps, only under the Git-ignored `cfg/ez2d2m/` (`identity-request-row.map`, `runtime-transform-inputs-run1.txt`, `runtime-transform-inputs-run2.txt`).

### What remains

**A valid `API_CODE` request row**, which is the external tool's to produce. reSoftlock currently has only a hypothesis path applying per-block `HL_CRYPT` to `0x0011`, which does not fit the shape; what is needed is a path that takes block 5, and block 3, and emits one request row, leaving the changing blocks `??` in the input. Whether 6th's seven inputs share this structure, which should be checked with repeated captures before re-judging. Observed but not pursued: all 32 of `ez2dj3rd`'s transforms report `unmapped=1` on this machine; the pre-change baseline shows the same, so it is not a regression from this change, but its cause was not examined. And the `ez2d2m` profile's `note` string still describes the state before task 244 ("raw I/O is disabled", stopping at `out dx, ax`); it was left alone as out of scope.
