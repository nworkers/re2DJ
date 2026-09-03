# Task 156: EZ2DJ 4th null-context 객체 상태·호출자 추적 작업 로그

## 결과 요약

field read 직전 경계 `0x0041a64c`에서 target object `0x00acd708`의 상태와 호출자 frame chain을 수집했습니다. 결과는 두 실행에서 재현되었고 다음이 확인됐습니다.

- **객체는 이미 초기화되어 있습니다.** 객체 base부터 `0x200`바이트 중 12개 dword가 0이 아니며, offset `0x00`에는 `.rdata` 주소가 들어 있습니다. 즉 생성자가 실행되어 vtable pointer가 설치된 객체입니다.
- **그 객체 안에서 `+0x11c`만 비어 있습니다.** 인접한 `+0x12c`, `+0xd4`, `+0x48` 등은 값이 채워져 있는데 target field만 `0x00000000`입니다.
- **호출자 경로 8단계를 확보했습니다.** 네 번의 hit 모두 동일한 frame chain을 기록했습니다.

따라서 "객체 전체가 미초기화"라는 가설은 폐기됩니다. 남은 문제는 생성자 이후 이 field 하나를 채워야 하는 별도 초기화 단계가 실행되지 않는다는 점입니다.

## 변경 사항

- `null_context_object_state.h/.cpp`를 새로 만들어 caller frame chain 수집과 객체 window 스캔을 분리했습니다. main.cpp에는 breakpoint 조율과 JSONL 기록만 남겼습니다.
- `--null-context-object-state-trace` 옵션과 usage 문자열을 추가했습니다.
- 경계 `image_base + 0x001a64c`를 `DR0` execution breakpoint로 설치하고, 새 thread에도 설치합니다. hit 상한 4에 도달하면 breakpoint를 해제합니다.
- hit, caller frame, 객체 window 요약, bounded nonzero entry를 JSONL로 기록합니다. 원본 메모리는 읽기만 합니다.
- 기존 하드웨어 추적 옵션과의 동시 사용을 거부하고, `ez2dj4th` 외 target에서는 실행을 거부합니다.
- 새 소스를 `re2dj_windows_original_process_backend` 빌드 대상에 추가했습니다.

## 검증 증거

- Windows x86 Debug 전체 빌드: 성공
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1184, failures: 0`
- 실제 CHD 실행: `20260903-121134-641.jsonl`, 재현 실행 `20260903-121242-364.jsonl` (둘 다 `--diagnostic-idle-timeout 60000`)
- 두 실행 모두 boundary가 `reason=child_exit`, `hits=4`, `recorded=4`, `frames=8`, `window_readable=true`, `disarmed=true`, `code=0xc0000005`입니다.

### 객체 window 요약

`readable=true`, `bytes=512`, `dwords=128`, `nonzero=12`, `field_offset=0x0000011c`, `field_scanned=true`, `field_value=0x00000000`, `capped=false`.

| offset | 값의 성격 | 비고 |
| --- | --- | --- |
| `0x000` | image 주소 (`.rdata` 범위) | vtable pointer로 판단 |
| `0x004` | 실행마다 다른 값 | 두 실행에서 서로 다름 |
| `0x008` | `1` | |
| `0x044` | `0x00004000` | |
| `0x048` | image 주소 (`.data` 범위) | Task 154에서 관찰된 객체 중 하나와 같은 주소 |
| `0x0d4` | `1` | |
| `0x11c` | `0x00000000` | **target field** |
| `0x12c` | `0x00ffffff` | |
| `0x1c4` | 실행마다 다른 값 | 상위 비트만 근소하게 변함 |
| `0x1d4` | 작은 정수 | |
| `0x1d8`–`0x1e0` | ASCII 문자열 field | 10자 이름 문자열 |

### 호출자 frame chain (네 hit 모두 동일)

| depth | 반환 주소 RVA | 섹션 |
| --- | --- | --- |
| 0 | `0x00071905` | `.text` |
| 1 | `0x00071867` | `.text` |
| 2 | `0x00071709` | `.text` |
| 3 | `0x000076ad` | `.text` |
| 4 | `0x000a4294` | `.text` |
| 5 | `0x000a4fbf` | `.text` |
| 6 | `0x00006834` | `.text` |
| 7 | `0x00006d3b` | `.text` |

네 hit 모두 `ECX = 0x00acd708`, `EBP = 0x0019fe04`로 같습니다. 즉 같은 호출자 경로에서 같은 객체로 최소 4회 진입합니다.

### PE 섹션 대조

`re2dj_pe_analyzer`로 확인한 원본 이미지 구성은 image base `0x00400000`, size of image `0x0071a000`이며 `.text` `0x00001000`(vsize `0x000db022`), `.rdata` `0x000dd000`, `.data` `0x000ea000`(vsize `0x005e66b0`, raw size `0x0001c000`)입니다. 이를 적용하면 다음이 확정됩니다.

- 객체 offset `0x00`의 값은 `.rdata` 범위이며, C++ 객체의 vtable pointer 형태와 일치합니다.
- target object RVA `0x006cd708`은 `.data`의 **파일 backing이 없는 뒤쪽 영역**에 있습니다. 즉 이미지 적재 시 0으로 채워지는 구간이며, 값은 런타임에만 생깁니다.
- 호출자 8단계의 반환 주소는 모두 `.text` 범위입니다.

### Task 154 분류 정정

Task 154에서 후보 receiver `0x00946d50`–`0x00948090`을 "heap 객체"로 기록했지만, size of image `0x0071a000` 기준으로 이 주소들은 RVA `0x00546d50`–`0x00548090`이며 `.data` 범위입니다. 따라서 **image-resident 정적 객체**이고 heap 할당이 아닙니다. 간격 `0x4d0`은 같은 클래스 정적 배열의 stride로 해석하는 것이 맞습니다. 이번 실행에서 target object의 offset `0x48`이 그중 하나(`0x009476f0`)를 가리키는 것도 확인됐습니다.

## 판정

- **확인됨 — 객체는 생성되었고 field 하나만 비어 있습니다.** vtable pointer와 다른 field들이 채워진 상태에서 `+0x11c`만 0입니다.
- **확인됨 — target object는 `.data`의 zero-fill 구간에 있습니다.** 파일에서 초기값을 받지 않으므로 이 field는 반드시 런타임 초기화가 필요합니다.
- **확인됨 — 호출자 경로가 결정적입니다.** 두 실행, 네 hit에서 8단계 frame chain이 동일합니다.
- **확인됨 — target object가 Task 154 후보 객체를 참조합니다.** offset `0x48`이 후보 1의 receiver 중 하나와 같은 주소입니다.
- **추정 — 누락된 것은 생성자 이후 단계입니다.** 생성자가 실행된 흔적이 있으므로, `+0x11c`는 생성자 이후 별도 초기화(예: 하위 객체 생성·등록 단계)에서 채워질 가능성이 큽니다. 이는 아직 실행 증거로 확인되지 않았습니다.
- **미확정 — 그 단계와 분기 조건.** 어떤 함수가 이 field를 채워야 하는지, 어떤 조건에서 건너뛰는지는 미확정입니다. field 직접 주입과 Hardlock 응답 변경은 계속 보류합니다.
- **주의 — `class` 값의 신뢰도.** window entry의 `stack` 분류는 `ESP ± 0x00100000` 휴리스틱이므로 실제 stack 여부를 증명하지 않습니다. `image` 분류만 PE 범위로 확정됩니다.

## 다음 단계

1. offset `0x00`의 vtable 주소를 기준으로 이 객체 클래스의 생성자와 method table을 식별합니다.
2. 호출자 depth 0–3(`0x00071905`, `0x00071867`, `0x00071709`, `0x000076ad`) 구간을 정적으로 확인해, field를 채워야 하는 초기화 호출이 어디서 갈라지는지 좁힙니다.
3. offset `0x48`이 가리키는 객체와 target object의 관계를 확인해, 두 정적 객체 배열의 초기화 순서를 파악합니다.

원본 CHD/HDD/EXE 내용과 Hardlock secret material은 이 문서와 저장소에 기록하지 않았습니다. 객체 window는 구조와 offset 요약으로만 기록했습니다.

---

# Task 156: EZ2DJ 4th Null-Context Object State and Caller Trace Work Log

## Result summary

The state of target object `0x00acd708` and the caller frame chain were collected at boundary `0x0041a64c`, immediately before the field read. The results reproduced across two runs and confirm the following.

- **The object is already initialized.** Twelve of the dwords in the first `0x200` bytes are nonzero, and offset `0x00` holds an `.rdata` address, so a constructor ran and installed a vtable pointer.
- **Only `+0x11c` is empty inside that object.** Neighboring fields such as `+0x12c`, `+0xd4`, and `+0x48` hold values while the target field is `0x00000000`.
- **An eight-level caller path was captured.** All four hits recorded the identical frame chain.

The hypothesis that the whole object is uninitialized is therefore rejected. What remains is that a separate post-construction step, which should fill this one field, does not run.

## Changes

- Added `null_context_object_state.h/.cpp` for caller frame-chain collection and object-window scanning, leaving only breakpoint orchestration and JSONL recording in main.cpp.
- Added the `--null-context-object-state-trace` option and usage text.
- Installed boundary `image_base + 0x001a64c` as a `DR0` execution breakpoint, including on new threads, and released it once the hit limit of four was reached.
- Recorded hits, caller frames, the object-window summary, and bounded nonzero entries as JSONL. Guest memory is only read.
- Rejected concurrent use with the existing hardware traces and refused targets other than `ez2dj4th`.
- Added the new source to the `re2dj_windows_original_process_backend` build target.

## Verification evidence

- Full Windows x86 Debug build: passed.
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1184, failures: 0`
- Real-CHD runs: `20260903-121134-641.jsonl` and the reproduction `20260903-121242-364.jsonl`, both with `--diagnostic-idle-timeout 60000`.
- Both runs report `reason=child_exit`, `hits=4`, `recorded=4`, `frames=8`, `window_readable=true`, `disarmed=true`, and `code=0xc0000005`.

### Object-window summary

`readable=true`, `bytes=512`, `dwords=128`, `nonzero=12`, `field_offset=0x0000011c`, `field_scanned=true`, `field_value=0x00000000`, `capped=false`.

| offset | nature of the value | note |
| --- | --- | --- |
| `0x000` | image address inside `.rdata` | read as a vtable pointer |
| `0x004` | varies per run | different in the two runs |
| `0x008` | `1` | |
| `0x044` | `0x00004000` | |
| `0x048` | image address inside `.data` | same address as one of the objects observed in Task 154 |
| `0x0d4` | `1` | |
| `0x11c` | `0x00000000` | **target field** |
| `0x12c` | `0x00ffffff` | |
| `0x1c4` | varies per run | only the high bits differ slightly |
| `0x1d4` | small integer | |
| `0x1d8`–`0x1e0` | ASCII string field | a ten-character name |

### Caller frame chain (identical for all four hits)

| depth | return-address RVA | section |
| --- | --- | --- |
| 0 | `0x00071905` | `.text` |
| 1 | `0x00071867` | `.text` |
| 2 | `0x00071709` | `.text` |
| 3 | `0x000076ad` | `.text` |
| 4 | `0x000a4294` | `.text` |
| 5 | `0x000a4fbf` | `.text` |
| 6 | `0x00006834` | `.text` |
| 7 | `0x00006d3b` | `.text` |

All four hits shared `ECX = 0x00acd708` and `EBP = 0x0019fe04`, so the same caller path enters with the same object at least four times.

### PE section cross-check

`re2dj_pe_analyzer` reports image base `0x00400000`, size of image `0x0071a000`, `.text` at `0x00001000` (vsize `0x000db022`), `.rdata` at `0x000dd000`, and `.data` at `0x000ea000` (vsize `0x005e66b0`, raw size `0x0001c000`). Applying that layout establishes the following.

- The value at object offset `0x00` lies in `.rdata` and matches the shape of a C++ vtable pointer.
- Target object RVA `0x006cd708` lies in the **file-unbacked tail of `.data`**, which the loader zero-fills, so its contents can only come from runtime initialization.
- All eight caller return addresses lie in `.text`.

### Correction to the Task 154 classification

Task 154 recorded candidate receivers `0x00946d50`–`0x00948090` as "heap objects", but with size of image `0x0071a000` those addresses are RVAs `0x00546d50`–`0x00548090`, inside `.data`. They are therefore **image-resident static objects**, not heap allocations, and the `0x4d0` spacing is best read as the stride of a static array of the same class. This run also shows that the target object's offset `0x48` points at one of them (`0x009476f0`).

## Classification

* **Confirmed — the object was constructed and only one field is empty.** The vtable pointer and other fields are populated while `+0x11c` alone is zero.
* **Confirmed — the target object lives in the zero-filled part of `.data`.** It receives no initial value from the file, so the field requires runtime initialization.
* **Confirmed — the caller path is deterministic.** The eight-level frame chain is identical across two runs and four hits.
* **Confirmed — the target object references a Task 154 candidate object.** Offset `0x48` holds the same address as one of candidate 1's receivers.
* **Inferred — what is missing is a post-construction step.** Because construction evidently ran, `+0x11c` is most likely filled by a separate later step such as sub-object creation or registration. This is not yet confirmed by execution evidence.
* **Unresolved — that step and its branch condition.** Which function should fill the field, and under what condition it is skipped, remain unknown. Direct field injection and Hardlock-response changes remain deferred.
* **Caveat — reliability of the `class` label.** The `stack` classification in window entries uses an `ESP ± 0x00100000` heuristic and does not prove stack residency. Only the `image` classification is established from the PE ranges.

## Next steps

1. Identify the class constructor and method table from the vtable address at offset `0x00`.
2. Inspect caller depths 0–3 (`0x00071905`, `0x00071867`, `0x00071709`, `0x000076ad`) statically to narrow where the initialization call that should fill the field diverges.
3. Determine the relationship between the object referenced at offset `0x48` and the target object to understand the initialization order of the two static object arrays.

No original CHD/HDD/EXE content or Hardlock secret material was recorded in this document or the repository. The object window is recorded only as a structural and offset summary.
