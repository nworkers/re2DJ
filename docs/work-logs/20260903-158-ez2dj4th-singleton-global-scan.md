# Task 158: EZ2DJ 4th singleton 전역 참조 스캔 작업 로그

## 결과 요약

전역 pointer `0x00ac29b4`는 복호화된 `.text`에서 **1210회** 참조됩니다. 같은 스캔에서 객체 주소 직접 참조는 3회, vtable 참조는 2회뿐입니다. 이 대비가 구조를 확정합니다. 이 객체는 코드 전반에서 전역을 통해 접근되는 subsystem singleton이며, 객체 주소 자체는 등록 지점과 두 개의 wrapper에서만 쓰입니다.

기록된 128개 참조 중 109개가 참조 직후 `call rel32`이고, 그 대상은 **40개의 서로 다른 함수**입니다. 모든 대상 RVA가 `0x00003a67` 이하로, `.text` 앞부분 thunk 구간에 몰려 있습니다.

## 변경 사항

- 공용 코어 `ImmediateReference`에 직후 바이트 창을 추가하고 leading 창과 폭을 맞춰 `kContextBytes = 8`로 통일했습니다.
- `ScanImmediateReferences`가 기록 상한을 넘어도 스캔을 계속해 `total_matches`로 총계를 돌려주도록 바꿨습니다. 상한은 이제 기록량만 제한합니다.
- 단위 테스트를 갱신했습니다. 직후 바이트 개수와 값, 버퍼 끝의 짧은 창, 상한 초과 시 총계 유지를 확인합니다.
- launcher probe의 참조 스캔을 값별 pass로 나누고 `null_context_object_reference_kind` 요약 이벤트를 추가했습니다.
- match 직후가 `e8`이면 `다음 명령 주소 + 5 + rel32`로 call 대상을 계산해 기록합니다.
- 전역 주소를 스캔 값에 추가하고, context 이벤트에 전역의 현재 값과 객체 주소 일치 여부를 기록합니다.

## 검증 증거

- Windows x86 Debug 전체 빌드: 성공
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1209, failures: 0`
- 실제 CHD 실행: `20260903-131003-927.jsonl` (`--diagnostic-idle-timeout 60000`)
- context 이벤트: `global=0x00ac29b4`, `global_value=0x00acd708`, `global_readable=true`, `global_matches_object=true`

### 값별 총계

| kind | 값 | 총계 | 기록 | capped |
| --- | --- | --- | --- | --- |
| object | `0x00acd708` | 3 | 3 | false |
| global | `0x00ac29b4` | 1210 | 128 | true |
| vtable | `0x004dd054` | 2 | 2 | false |

### 기록된 전역 참조 128개의 형태

| 직전 두 바이트 | 형태 | 건수 |
| --- | --- | --- |
| `8b 0d` | `mov ecx, [global]` | 115 |
| `8b 15` | `mov edx, [global]` | 6 |
| `a1` | `mov eax, [global]` | 7 |

직후에 `call rel32`가 오는 경우가 109건, 아닌 경우가 19건입니다. 해석된 대상은 40개이며 가장 많이 불리는 것은 `RVA 0x00001ac3`(15건), 이어서 `0x00003a67`(13건), `0x00001a41`(12건), `0x000032ba`(11건)입니다.

### 참조 분포

기록된 전역 참조는 `RVA 0x00036072`부터 `0x0003d89e`까지 한 구간에 몰려 있습니다. 상한 때문에 그 뒤 구간은 기록되지 않았지만 총계 1210은 온전합니다.

## 판정

- **확인됨 — 이 객체는 전역을 통해 접근되는 singleton입니다.** 전역 참조 1210회 대 객체 주소 직접 참조 3회입니다. 객체 주소 3회는 Task 157에서 확인한 등록 1회와 receiver 적재 2회입니다.
- **확인됨 — 전역은 경계 시점에 객체를 가리킵니다.** `global_value=0x00acd708`, `global_matches_object=true`입니다.
- **확인됨 — 대부분의 참조가 thiscall receiver 적재입니다.** 기록된 128건 중 115건이 `mov ecx, [global]`이고, 그중 다수가 곧바로 `call`로 이어집니다.
- **확인됨 — `0x00401ac3`은 경계 함수로 가는 thunk입니다.** 이 대상은 전역 receiver 적재 뒤 가장 많이 호출되며(15건), Task 157의 호출자 창에서 `call 0x00401ac3`의 반환 주소 `0x00471905`가 경계 함수 frame chain의 depth 0입니다. 중간에 frame이 생기지 않았으므로 이 호출은 `0x0041a649`로 이어집니다.
- **추정 — 낮은 RVA 대상은 incremental-link thunk 표입니다.** 해석된 40개 대상이 모두 `0x00003a67` 이하이며 실제 함수 본문은 그보다 훨씬 뒤에 있습니다. thunk 바이트 자체는 아직 읽지 않았습니다.
- **추정 — 이 singleton은 그래픽·장치 계열 관리자입니다.** 호출 인자에 `0x437f0000`(255.0), `0x41200000`(10.0) 같은 단정도 상수와 좌표성 정수가 자주 등장하고, vtable 인접 `.rdata`에도 같은 성격의 상수가 있습니다. 클래스 이름이나 역할을 직접 확인한 것은 아닙니다.
- **판정 — `+0x11c` 초기화 후보 범위가 좁아졌습니다.** field를 채우는 코드는 이 40개 진입점 중 하나를 통해 receiver를 받은 함수 안에 있어야 합니다. Task 154에서 `.text` 전체의 `+0x11c` write 명령이 네 개뿐임을 확인했으므로, 그 네 명령 중 하나가 이 singleton을 receiver로 실행되는 경로가 존재해야 합니다. 관찰 구간에서는 그 경로가 실행되지 않았습니다.
- **미확정 — 그 경로와 건너뛰는 조건.** 여전히 미확정이며 field 직접 주입과 Hardlock 응답 변경은 계속 보류합니다.

## 다음 단계

1. 경계 `0x0041a64c`부터 field read anchor `0x0041a699`까지 함수 본문을 코드 창으로 읽어, field를 읽기 전에 초기화 여부를 검사하는 분기가 있는지 확인합니다.
2. Task 154의 write 후보 두 개(`RVA 0x0000fdbd`, `0x0000fde1`)를 포함하는 함수의 경계를 찾아, 그 함수가 이 singleton의 초기화 경로에서 호출되는지 확인합니다.
3. 필요하면 기록 상한을 올려 전역 참조의 뒤쪽 구간까지 확보합니다.

원본 CHD/HDD/EXE 내용과 Hardlock secret material은 이 문서와 저장소에 기록하지 않았습니다.

---

# Task 158: EZ2DJ 4th Singleton Global Reference Scan Work Log

## Result summary

The global pointer `0x00ac29b4` is referenced **1210 times** in the decrypted `.text`. In the same scan, the object address itself appears three times and the vtable twice. That contrast settles the structure: the object is a subsystem singleton reached through the global across the code base, and its address is used only at the registration site and two wrappers.

Of the 128 recorded references, 109 are directly followed by `call rel32`, resolving to **40 distinct functions**. Every resolved target has an RVA at or below `0x00003a67`, clustered in the thunk region at the front of `.text`.

## Changes

- Added a trailing-byte window to the shared-core `ImmediateReference` and unified both windows as `kContextBytes = 8`.
- Made `ScanImmediateReferences` keep scanning past the record cap and return the full count through `total_matches`, so the cap now limits only how much is recorded.
- Updated the unit tests for trailing-byte counts and values, short windows at the buffer end, and total preservation past the cap.
- Split the launcher probe's reference scan into one pass per value and added the `null_context_object_reference_kind` summary event.
- When a match is followed by `e8`, the call target is computed as `next instruction address + 5 + rel32` and recorded.
- Added the global address to the scanned values and recorded its current value and object-address equality in the context event.

## Verification evidence

- Full Windows x86 Debug build: passed.
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1209, failures: 0`
- Real-CHD run: `20260903-131003-927.jsonl` with `--diagnostic-idle-timeout 60000`.
- Context event: `global=0x00ac29b4`, `global_value=0x00acd708`, `global_readable=true`, `global_matches_object=true`.

### Per-value totals

| kind | value | total | recorded | capped |
| --- | --- | --- | --- | --- |
| object | `0x00acd708` | 3 | 3 | false |
| global | `0x00ac29b4` | 1210 | 128 | true |
| vtable | `0x004dd054` | 2 | 2 | false |

### Forms of the 128 recorded global references

| preceding two bytes | form | count |
| --- | --- | --- |
| `8b 0d` | `mov ecx, [global]` | 115 |
| `8b 15` | `mov edx, [global]` | 6 |
| `a1` | `mov eax, [global]` | 7 |

A `call rel32` follows in 109 cases and does not in 19. The resolved targets number 40, the most frequent being `RVA 0x00001ac3` (15), then `0x00003a67` (13), `0x00001a41` (12), and `0x000032ba` (11).

### Reference distribution

The recorded global references fall in one span from `RVA 0x00036072` to `0x0003d89e`. The cap left the region beyond that unrecorded, but the total of 1210 is complete.

## Classification

* **Confirmed — the object is a singleton reached through the global.** There are 1210 global references against three direct object-address references, the latter being Task 157's one registration and two receiver loads.
* **Confirmed — the global points at the object at the boundary.** `global_value=0x00acd708` with `global_matches_object=true`.
* **Confirmed — most references are thiscall receiver loads.** 115 of the 128 recorded are `mov ecx, [global]`, and most are immediately followed by a call.
* **Confirmed — `0x00401ac3` is the thunk into the boundary function.** It is the most frequent target after a global receiver load (15 sites), and in Task 157's caller window the return address of `call 0x00401ac3`, `0x00471905`, is depth 0 of the boundary function's frame chain. No intervening frame was created, so that call reaches `0x0041a649`.
* **Inferred — the low-RVA targets are an incremental-link thunk table.** All 40 resolved targets are at or below `0x00003a67` while the actual function bodies lie much further on. The thunk bytes have not been read.
* **Inferred — the singleton is a graphics or device manager.** Call arguments frequently contain single-precision constants such as `0x437f0000` (255.0) and `0x41200000` (10.0) alongside coordinate-like integers, and the `.rdata` adjacent to the vtable holds constants of the same character. The class name and role were not directly established.
* **Classification — the range for `+0x11c` initialization has narrowed.** The code that fills the field must live in a function that received the receiver through one of these 40 entry points. Since Task 154 confirmed that the whole `.text` contains only four `+0x11c` write instructions, a path must exist where one of those four executes with this singleton as its receiver. That path did not run in the observed window.
* **Unresolved — that path and the condition that skips it.** Direct field injection and Hardlock-response changes remain deferred.

## Next steps

1. Read the function body from boundary `0x0041a64c` to field-read anchor `0x0041a699` as a code window and check whether a branch tests the field's initialization before reading it.
2. Locate the bounds of the function containing Task 154's two write candidates (`RVA 0x0000fdbd`, `0x0000fde1`) and check whether it is called on this singleton's initialization path.
3. Raise the record cap if the later span of global references is needed.

No original CHD/HDD/EXE content or Hardlock secret material was recorded in this document or the repository.
