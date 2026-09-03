# Task 157: EZ2DJ 4th null-context 객체 참조 스캔 작업 로그

## 결과 요약

복호화된 `.text` 전체(`0x000db022`바이트)에서 target object `0x00acd708`과 그 vtable `0x004dd054`를 immediate로 갖는 지점은 **모두 5개**였습니다. 두 실행에서 동일하게 재현됐습니다. 이 다섯 지점과 vtable slot, 호출자 코드 창을 합치면 객체 접근 구조가 드러납니다.

- 이 객체는 전역 pointer `0x00ac29b4`에 등록되는 **단일 인스턴스**이며, 호출자는 객체 주소가 아니라 그 전역에서 receiver를 읽습니다.
- vtable은 `.rdata` RVA `0x000dd054`에 있고 `.text` 주소를 담은 slot은 4개입니다.
- 생성자 쪽 vtable 설치 지점 2개를 확인했습니다.
- `+0x11c`를 쓰는 지점은 이 다섯 참조 어디에도 없습니다.

## 변경 사항

- 공용 코어에 `include/re2dj/exe/immediate_scan.h`와 `src/exe/immediate_scan.cpp`를 추가했습니다. `ScanImmediateReferences`는 명령 디코더 없이 32비트 little-endian immediate를 찾아 offset, 값, 직전 4바이트를 반환하고 상한 초과를 `capped`로 보고하는 순수 함수입니다.
- `tests/unit/immediate_scan_test.cpp`를 추가했습니다. 버퍼 시작 부근 match, 상한 절단, 값 부재, 짧은 버퍼, 빈 값 목록, null 입력을 확인합니다.
- launcher probe에 `--null-context-object-reference-scan` 옵션을 추가했습니다. 경계 `image_base + 0x001a64c`에서 한 번만 수집하고 breakpoint를 해제합니다.
- vtable slot 16개, immediate match, caller 코드 창 8개, 스캔 요약을 JSONL로 기록합니다. guest 메모리는 읽기만 합니다.
- 기존 하드웨어 추적 옵션 및 Task 156 옵션과의 동시 사용을 거부하고, `ez2dj4th` 외 target을 거부합니다.

## 검증 증거

- Windows x86 Debug 전체 빌드: 성공
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1202, failures: 0` (Task 156 시점 1184에서 18개 증가)
- 실제 CHD 실행: `20260903-124903-472.jsonl`, 재현 실행 `20260903-125055-989.jsonl` (둘 다 `--diagnostic-idle-timeout 60000`)
- 두 실행의 vtable slot, match, caller 창 이벤트는 완전히 동일합니다. boundary는 `reason=child_exit`, `hits=1`, `scanned=true`, `text_readable=true`, `matches=5`, `capped=false`, `code=0xc0000005`입니다.

### immediate match 5개

| RVA | 값 | 직전 4바이트 | 바이트로 확인되는 형태 |
| --- | --- | --- | --- |
| `0x00010381` | `0x004dd054` | `45 fc c7 00` | `c7 00 <vtable>` = `mov [eax], vtable` |
| `0x000104a1` | `0x004dd054` | `45 f0 c7 00` | `c7 00 <vtable>` = `mov [eax], vtable` |
| `0x000a295c` | `0x00acd708` | `55 8b ec b9` | 함수 prologue 직후 `b9 <obj>` = `mov ecx, obj` |
| `0x000a298b` | `0x00acd708` | `55 8b ec b9` | 함수 prologue 직후 `b9 <obj>` = `mov ecx, obj` |
| `0x000a2b28` | `0x00acd708` | `b4 29 ac 00` | 앞 dword가 `0x00ac29b4`, `c7 05 <global> <obj>` 형태 |

### vtable slot

`.rdata` `0x004dd054`부터 16 slot을 읽었습니다. slot 0–3은 `.text` 주소(`RVA 0x0000111d`, `0x00002603`, `0x00001046`, `0x00002126`)이고, slot 4는 `0x00000000`이며, slot 5–12는 IEEE-754 단정도 상수 패턴(`0x40490fdb`, `0x3e4ccccd`, `0x4053d70a`, `0x41200000`, `0x41300000`, `0x40c00000`, `0x40800000`, `0x437f0000`)입니다. slot 13–14는 다시 `.text` 주소입니다.

### 호출자 코드 창

depth 0의 32바이트 창을 디코드하면 반환 주소 `0x00471905` 직전은 다음과 같습니다.

```
0x004718ee  8b 55 f0              mov  edx, [ebp-0x10]
0x004718f1  83 c2 18              add  edx, 0x18
0x004718f4  52                    push edx
0x004718f5  68 64 99 4f 00        push 0x004f9964
0x004718fa  8b 0d b4 29 ac 00     mov  ecx, [0x00ac29b4]
0x00471900  e8 be 01 f9 ff        call 0x00401ac3
0x00471905                        <- 반환 주소
```

depth 1의 창에는 `83 7d ec 00` / `74 0d`(`cmp [ebp-0x14], 0` 후 조건 분기)가 있어 호출 앞에 null 검사가 있습니다. depth 3의 창에는 `c7 42 08 01 00 00 00`(`mov [edx+8], 1`)과 `ff 52 10`(`call [edx+0x10]`, vtable slot index 4를 통한 가상 호출)이 있습니다.

## 판정

- **확인됨 — `.text` 전체에서 객체 참조는 3개뿐입니다.** 스캔은 `readable=true`, `capped=false`이며 객체 주소 match는 `0x000a295c`, `0x000a298b`, `0x000a2b28` 세 곳입니다.
- **확인됨 — vtable 설치 지점이 2개입니다.** `0x00010381`과 `0x000104a1`의 직전 바이트가 모두 `c7 00`이므로 `mov [eax], 0x004dd054` 형태입니다.
- **확인됨 — 호출자는 전역 pointer에서 receiver를 읽습니다.** depth 0 창의 `8b 0d b4 29 ac 00`은 `mov ecx, [0x00ac29b4]`이고 그 다음 명령이 `call`입니다.
- **확인됨 — 그 전역은 이 호출 시점에 객체 주소를 담고 있습니다.** 같은 실행에서 경계 hit의 `ECX`가 `0x00acd708`이었고, 호출과 경계 사이에는 이 한 번의 call만 있습니다.
- **추정 — `0x000a2b28`은 그 전역에 객체 주소를 저장하는 지점입니다.** match 직전 dword가 정확히 `0x00ac29b4`이므로 `c7 05 b4 29 ac 00 08 d7 ac 00`(`mov [0x00ac29b4], offset obj`) 형태로 읽힙니다. 앞의 `c7 05` 두 바이트는 이번 수집 범위(직전 4바이트) 밖이라 직접 확인하지는 않았습니다.
- **추정 — `0x00401ac3`은 incremental-link thunk입니다.** vtable slot과 여러 call 대상이 모두 `.text` 앞부분의 낮은 주소에 몰려 있어 jump thunk 표로 보입니다. 이 경우 `ECX`는 thunk를 통과하며 보존됩니다. thunk 바이트 자체는 아직 읽지 않았습니다.
- **추정 — vtable은 slot 4개입니다.** slot 4가 0이고 slot 5 이후가 부동소수 상수 패턴이므로, 그 앞까지가 이 클래스의 method table로 보입니다. RTTI나 인접 vtable 경계는 확인하지 않았습니다.
- **확인됨 — 다섯 참조 중 `+0x11c` write는 없습니다.** 객체 주소를 immediate로 쓰는 세 지점은 receiver 적재 두 곳과 전역 저장 한 곳이며, field write 형태가 아닙니다. Task 146(절대 주소 `matches=0`), Task 154(후보 target match 0건)와 합치면 이 field를 채우는 코드는 **객체 주소를 immediate로 갖지 않는 경로**에서만 나올 수 있습니다. 즉 전역 pointer를 거쳐 receiver를 받은 함수 안에서 쓰여야 합니다.
- **미확정 — 그 함수와 실행되지 않는 조건.** field를 채워야 하는 함수와 건너뛰는 분기는 여전히 미확정입니다. field 직접 주입과 Hardlock 응답 변경은 계속 보류합니다.

## 다음 단계

1. 전역 pointer `0x00ac29b4`를 읽는 지점을 같은 방식으로 스캔해, 이 단일 인스턴스를 receiver로 받는 함수 집합을 모읍니다. 그 집합이 `+0x11c` write 후보와 겹치는지 확인합니다.
2. vtable slot 0–3의 함수와 생성자 두 지점(`0x00010381`, `0x000104a1`) 주변을 코드 창으로 읽어 클래스 구조를 좁힙니다.
3. depth 1의 null 검사와 depth 3의 가상 호출이 초기화 분기와 연결되는지 확인합니다.

원본 CHD/HDD/EXE 내용과 Hardlock secret material은 이 문서와 저장소에 기록하지 않았습니다. 코드 창은 판정에 필요한 범위의 명령 디코드로만 인용했습니다.

---

# Task 157: EZ2DJ 4th Null-Context Object Reference Scan Work Log

## Result summary

Across the complete decrypted `.text` (`0x000db022` bytes), exactly **five** sites carry target object `0x00acd708` or its vtable `0x004dd054` as an immediate. The set reproduced across two runs. Together with the vtable slots and caller code windows, these five reveal the object's access structure.

- The object is a **single instance** registered in the global pointer `0x00ac29b4`, and callers load the receiver from that global rather than from the object address.
- The vtable is at `.rdata` RVA `0x000dd054` and has four slots holding `.text` addresses.
- Two constructor-side vtable installations were identified.
- None of the five references writes `+0x11c`.

## Changes

- Added `include/re2dj/exe/immediate_scan.h` and `src/exe/immediate_scan.cpp` to the shared core. `ScanImmediateReferences` is a pure function that finds 32-bit little-endian immediates without an instruction decoder, returning each match's offset, value, and preceding four bytes, and reporting truncation through `capped`.
- Added `tests/unit/immediate_scan_test.cpp`, covering matches near the buffer start, cap truncation, absent values, short buffers, empty value lists, and null input.
- Added `--null-context-object-reference-scan` to the launcher probe. It collects once at boundary `image_base + 0x001a64c` and releases the breakpoint.
- Records sixteen vtable slots, immediate matches, eight caller code windows, and a scan summary as JSONL. Guest memory is read-only.
- Rejects concurrent use with the existing hardware traces and the Task 156 option, and refuses targets other than `ez2dj4th`.

## Verification evidence

- Full Windows x86 Debug build: passed.
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1202, failures: 0` (18 more than the 1184 at Task 156).
- Real-CHD runs: `20260903-124903-472.jsonl` and the reproduction `20260903-125055-989.jsonl`, both with `--diagnostic-idle-timeout 60000`.
- The vtable-slot, match, and caller-window events are identical across both runs. The boundary reports `reason=child_exit`, `hits=1`, `scanned=true`, `text_readable=true`, `matches=5`, `capped=false`, and `code=0xc0000005`.

### The five immediate matches

| RVA | value | preceding four bytes | form confirmed from bytes |
| --- | --- | --- | --- |
| `0x00010381` | `0x004dd054` | `45 fc c7 00` | `c7 00 <vtable>` = `mov [eax], vtable` |
| `0x000104a1` | `0x004dd054` | `45 f0 c7 00` | `c7 00 <vtable>` = `mov [eax], vtable` |
| `0x000a295c` | `0x00acd708` | `55 8b ec b9` | `b9 <obj>` = `mov ecx, obj`, right after a function prologue |
| `0x000a298b` | `0x00acd708` | `55 8b ec b9` | `b9 <obj>` = `mov ecx, obj`, right after a function prologue |
| `0x000a2b28` | `0x00acd708` | `b4 29 ac 00` | preceding dword is `0x00ac29b4`, a `c7 05 <global> <obj>` shape |

### Vtable slots

Sixteen slots were read from `.rdata` `0x004dd054`. Slots 0–3 hold `.text` addresses (`RVA 0x0000111d`, `0x00002603`, `0x00001046`, `0x00002126`), slot 4 is `0x00000000`, and slots 5–12 follow an IEEE-754 single-precision constant pattern (`0x40490fdb`, `0x3e4ccccd`, `0x4053d70a`, `0x41200000`, `0x41300000`, `0x40c00000`, `0x40800000`, `0x437f0000`). Slots 13–14 hold `.text` addresses again.

### Caller code windows

Decoding depth 0's 32-byte window, the code immediately before return address `0x00471905` is:

```
0x004718ee  8b 55 f0              mov  edx, [ebp-0x10]
0x004718f1  83 c2 18              add  edx, 0x18
0x004718f4  52                    push edx
0x004718f5  68 64 99 4f 00        push 0x004f9964
0x004718fa  8b 0d b4 29 ac 00     mov  ecx, [0x00ac29b4]
0x00471900  e8 be 01 f9 ff        call 0x00401ac3
0x00471905                        <- return address
```

Depth 1's window contains `83 7d ec 00` and `74 0d` (`cmp [ebp-0x14], 0` followed by a conditional branch), so a null check precedes that call. Depth 3's window contains `c7 42 08 01 00 00 00` (`mov [edx+8], 1`) and `ff 52 10` (`call [edx+0x10]`, a virtual call through vtable slot index 4).

## Classification

* **Confirmed — the whole `.text` contains only three object references.** The scan reports `readable=true` and `capped=false`, with object-address matches at `0x000a295c`, `0x000a298b`, and `0x000a2b28`.
* **Confirmed — there are two vtable installation sites.** Both `0x00010381` and `0x000104a1` are preceded by `c7 00`, giving `mov [eax], 0x004dd054`.
* **Confirmed — callers load the receiver from a global pointer.** Depth 0's window contains `8b 0d b4 29 ac 00`, that is `mov ecx, [0x00ac29b4]`, immediately followed by the call.
* **Confirmed — that global holds the object address at this call.** In the same run, `ECX` was `0x00acd708` at the boundary hit, and only this one call lies between the load and the boundary.
* **Inferred — `0x000a2b28` is where the object address is stored into that global.** The dword preceding the match is exactly `0x00ac29b4`, giving the shape `c7 05 b4 29 ac 00 08 d7 ac 00` (`mov [0x00ac29b4], offset obj`). The leading `c7 05` lies outside the four preceding bytes collected here and was not read directly.
* **Inferred — `0x00401ac3` is an incremental-link thunk.** Vtable slots and several call targets cluster in the low part of `.text`, which reads as a jump-thunk table; in that case `ECX` survives the thunk. The thunk bytes have not been read yet.
* **Inferred — the vtable has four slots.** Slot 4 is zero and slots 5 onward follow a floating-point constant pattern, so the class method table appears to end before them. RTTI and adjacent vtable boundaries were not examined.
* **Confirmed — none of the five references writes `+0x11c`.** The three object-address sites are two receiver loads and one global store, none of them a field write. Combined with Task 146 (`matches=0` for the absolute field address) and Task 154 (zero candidate target matches), the code that fills this field can only exist on a path that **does not carry the object address as an immediate**, that is, inside a function that received the receiver through the global pointer.
* **Unresolved — that function and the condition that skips it.** Direct field injection and Hardlock-response changes remain deferred.

## Next steps

1. Scan for reads of the global pointer `0x00ac29b4` the same way, to collect the set of functions that take this single instance as a receiver, and check whether that set overlaps the `+0x11c` write candidates.
2. Read code windows around vtable slots 0–3 and the two constructor sites (`0x00010381`, `0x000104a1`) to narrow the class structure.
3. Determine whether depth 1's null check and depth 3's virtual call connect to the initialization branch.

No original CHD/HDD/EXE content or Hardlock secret material was recorded in this document or the repository. Code windows are quoted only as the instruction decoding needed for the classification.
