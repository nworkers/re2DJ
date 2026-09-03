# Task 159: EZ2DJ 4th 코드 영역 스캔 작업 로그

## 결과 요약

두 가지가 확정됐습니다.

- **field를 읽는 함수는 읽기 전에 아무 검사도 하지 않습니다.** `[this+0x11c]`를 `ECX`로 읽고 곧바로 thiscall로 호출합니다. 코드는 이 field가 항상 유효하다고 가정합니다.
- **실행되는 두 write 후보는 같은 함수 안에 있고, 대상은 singleton이 아니라 정적 배열의 원소입니다.** 그 함수는 `0x00946d50 + index * 0x4d0`으로 원소 주소를 계산하고 `0x4d0`바이트를 memset한 뒤 template에서 필드를 복사합니다. 후보 두 개는 그 원소의 `+0x11c`를 if/else 양쪽에서 각각 채웁니다.

이로써 "왜 네 후보가 singleton의 field를 쓰지 않는가"가 코드 수준에서 설명됩니다. 그 후보들은 다른 클래스의 원소 초기화 코드입니다.

## 변경 사항

- 공용 코어에 `include/re2dj/exe/code_scan.h`와 `src/exe/code_scan.cpp`를 추가했습니다. `FindPrologueBefore`는 anchor 앞쪽으로 `55 8b ec`를 찾는 순수 함수이며, anchor 자신을 제외해 함수 시작이 길이 0으로 보고되지 않게 합니다.
- `tests/unit/code_scan_test.cpp`를 추가했습니다. 함수 내부 anchor, prologue 위 anchor, 검색 범위 초과, 빈 결과, null 입력을 확인합니다.
- 참조 스캔 뒤 다섯 anchor의 함수 시작과 코드 영역을 `null_context_code_region` 이벤트로 기록합니다.
- anchor가 함수 시작 창(`0xc0`바이트) 밖이면 anchor 중심 창(앞 `0x20`, 뒤 `0x10`)을 추가로 기록합니다.
- 창은 이미 읽어 둔 `.text` 복사본에서 잘라내므로 추가 원격 읽기는 없습니다.

## 검증 증거

- Windows x86 Debug 전체 빌드: 성공
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1219, failures: 0` (Task 158 시점 1209에서 10개 증가)
- 실제 CHD 실행: `20260903-134824-522.jsonl`, anchor 창 추가 후 `20260903-135023-263.jsonl` (둘 다 `--diagnostic-idle-timeout 60000`)

### 함수 시작

| anchor | RVA | 함수 시작 RVA | 거리 |
| --- | --- | --- | --- |
| field_read | `0x0001a699` | `0x0001a649` | 80 |
| write_candidate_0 | `0x0000fdbd` | `0x0000fc57` | 358 |
| write_candidate_1 | `0x0000fde1` | `0x0000fc57` | 394 |
| vtable_store_0 | `0x00010381` | `0x00010366` | 27 |
| vtable_store_1 | `0x000104a1` | `0x00010479` | 40 |

두 write 후보는 **같은 함수**(`0x0000fc57`) 안에 있습니다.

### field read 함수 본문

```
0041a649  55                    push ebp
0041a64a  8b ec                 mov  ebp, esp
0041a64c  81 ec 20 01 00 00     sub  esp, 0x120          <- 경계 breakpoint 위치
          56 57 51              push esi, edi, ecx
          8d bd e0 fe ff ff     lea  edi, [ebp-0x120]
          b9 48 00 00 00        mov  ecx, 0x48
          b8 cc cc cc cc        mov  eax, 0xcccccccc
          f3 ab                 rep  stosd               <- debug 빌드 스택 채우기
          59                    pop  ecx
          89 8d e8 fe ff ff     mov  [ebp-0x118], ecx    <- this 저장
          83 7d 10 00           cmp  dword ptr [ebp+0x10], 0
          74 0b                 je   +0x0b
          8b 45 10              mov  eax, [ebp+0x10]
          89 85 e4 fe ff ff     mov  [ebp-0x11c], eax
          eb 09                 jmp  +0x09
          8b 4d 08              mov  ecx, [ebp+0x08]
          89 8d e4 fe ff ff     mov  [ebp-0x11c], ecx
          8d 55 f4              lea  edx, [ebp-0x0c]
          52                    push edx
          8b 85 e4 fe ff ff     mov  eax, [ebp-0x11c]
          50                    push eax
          8b 8d e8 fe ff ff     mov  ecx, [ebp-0x118]    <- this
0041a699  8b 89 1c 01 00 00     mov  ecx, [ecx+0x11c]    <- field read
          e8 f4 7b fe ff        call 0x00402298
          85 c0                 test eax, eax
          74 12                 je   +0x12
```

함수의 유일한 분기는 인자 선택(`[ebp+0x10]`이 0인지)이며, field 자체에 대한 검사는 없습니다.

### write 후보 함수 본문

`0x0000fc57` 함수의 시작부는 다음을 수행합니다.

```
          a1 98 cd 94 00        mov  eax, [0x0094cd98]
          83 c0 01              add  eax, 1
          a3 98 cd 94 00        mov  [0x0094cd98], eax    <- 카운터 증가
          8b 4d 14              mov  ecx, [ebp+0x14]
          89 4d fc              mov  [ebp-0x04], ecx      <- template 원본
          8b 15 9c cd 94 00     mov  edx, [0x0094cd9c]
          69 d2 d0 04 00 00     imul edx, edx, 0x4d0      <- stride
          81 c2 50 6d 94 00     add  edx, 0x00946d50      <- 배열 base
          89 55 f4              mov  [ebp-0x0c], edx      <- 새 원소
          68 d0 04 00 00        push 0x4d0
          6a 00                 push 0
          8b 45 f4 50           push [ebp-0x0c]
          e8 84 26 0b 00        call memset
```

이어서 `[원소+0x118]`에 플래그를 쓰고, template에서 `0xec`바이트를 `원소+0x2c`로 복사하며, `+0x4c8`을 복사하고 `+0x120`부터 `0x17c`바이트를 `rep movsd`로 복사합니다.

두 후보는 if/else 양쪽입니다.

```
0040fdbd  8b 45 f4              mov  eax, [ebp-0x0c]
          05 ac 04 00 00        add  eax, 0x4ac
          8b 4d f4              mov  ecx, [ebp-0x0c]
          89 81 1c 01 00 00     mov  [ecx+0x11c], eax     <- 후보 0: 원소 내부 pointer
          ...                   push 0x27, template, 원소
          ff 15 24 17 ad 00     call dword ptr [0x00ad1724]
          eb 26                 jmp  +0x26
0040fde1  8b 4d f4              mov  ecx, [ebp-0x0c]
          c7 81 1c 01 00 00 00 00 00 00   mov dword ptr [ecx+0x11c], 0   <- 후보 1
```

### vtable 설치 함수

```
00010366  55 8b ec              push ebp; mov ebp, esp
          51                    push ecx
          c7 45 fc cc cc cc cc  mov  [ebp-0x04], 0xcccccccc
          89 4d fc              mov  [ebp-0x04], ecx      <- this
          8b 4d fc              mov  ecx, [ebp-0x04]
          e8 df 15 ff ff        call <base constructor>
          8b 45 fc              mov  eax, [ebp-0x04]
00010381  c7 00 54 d0 4d 00     mov  dword ptr [eax], 0x004dd054   <- vtable 설치
          8b 4d fc              mov  ecx, [ebp-0x04]
          c7 41 04 00 00 00 00  mov  dword ptr [ecx+0x04], 0
```

## 판정

- **확인됨 — field 읽기 전 검사가 없습니다.** 함수 본문의 유일한 분기는 인자 선택이며, `[this+0x11c]`는 읽자마자 `call 0x00402298`의 receiver로 쓰입니다.
- **확인됨 — 두 write 후보는 같은 함수 `0x0000fc57`에 속합니다.** 함수 시작 검색이 두 anchor 모두 같은 prologue를 가리켰습니다.
- **확인됨 — 그 함수는 정적 배열 원소를 초기화합니다.** `imul edx, edx, 0x4d0`과 `add edx, 0x00946d50`으로 원소 주소를 계산하고 `0x4d0`바이트를 memset합니다. 이것이 Task 154에서 관찰된 receiver `0x00946d50`, `0x00947220`, `0x009476f0`, `0x00947bc0`, `0x00948090`과 간격 `0x4d0`의 근거입니다.
- **확인됨 — singleton의 `+0x48`은 그 배열의 index 2 원소입니다.** `0x00946d50 + 2 * 0x4d0 = 0x009476f0`이며, Task 156이 기록한 값과 같습니다.
- **확인됨 — 후보 두 개는 if/else 양쪽에서 원소의 `+0x11c`를 채웁니다.** 한쪽은 `원소 + 0x4ac`를, 다른 쪽은 0을 씁니다. 두 분기 모두 직후에 `0x00ad1724`를 통한 간접 호출을 수행합니다.
- **확인됨 — vtable 설치 함수는 생성자 형태입니다.** base 생성자 호출, vtable 설치, `+0x04` 0 초기화 순서입니다.
- **추정 — `0x00ad1724`는 IAT slot입니다.** 원본 이미지의 `.idata`는 RVA `0x006d1000`, 크기 `0x0000171c`이므로 이 주소(RVA `0x006d1724`)는 그 범위 안입니다. 어떤 import인지는 확인하지 않았습니다.
- **판정 — 네 후보가 singleton을 쓰지 않는 이유가 코드로 설명됩니다.** 후보 0·1은 다른 클래스의 배열 원소 초기화 코드이며 receiver가 구조적으로 그 배열 원소입니다. 따라서 singleton의 `+0x11c`는 `.text` 안에 전용 writer가 없는 상태이고, 값이 채워지려면 후보 2·3(`0x0001825f`, `0x0001dbd3`) 중 하나가 singleton을 receiver로 실행되어야 합니다. 두 후보는 관찰 구간에서 한 번도 실행되지 않았습니다.
- **미확정 — 후보 2·3의 소속 함수와 호출 조건.** 다음 조사 대상입니다. field 직접 주입과 Hardlock 응답 변경은 계속 보류합니다.

## 다음 단계

1. 실행되지 않은 write 후보 `0x0001825f`와 `0x0001dbd3`의 함수 시작과 본문을 같은 방식으로 수집합니다.
2. 그 함수들이 singleton 진입점 40개 중 어디에서 호출되는지 확인합니다.
3. 필요하면 `0x00ad1724` IAT slot의 import 이름을 확인해 원소 초기화 경로의 외부 의존을 파악합니다.

원본 CHD/HDD/EXE 내용과 Hardlock secret material은 이 문서와 저장소에 기록하지 않았습니다. 코드 인용은 판정에 필요한 명령 디코드로 제한했습니다.

---

# Task 159: EZ2DJ 4th Code Region Scan Work Log

## Result summary

Two things are now settled.

- **The function that reads the field checks nothing beforehand.** It loads `[this+0x11c]` into `ECX` and immediately makes a thiscall through it. The code assumes the field is always valid.
- **The two executing write candidates live in the same function, and their target is an element of a static array rather than the singleton.** That function computes the element address as `0x00946d50 + index * 0x4d0`, memsets `0x4d0` bytes, and copies fields from a template. The two candidates fill that element's `+0x11c` on the two sides of an if/else.

This explains at code level why the four candidates never write the singleton's field: they are element-initialization code for a different class.

## Changes

- Added `include/re2dj/exe/code_scan.h` and `src/exe/code_scan.cpp` to the shared core. `FindPrologueBefore` searches backward from an anchor for `55 8b ec`, excluding the anchor itself so a function start is never reported with zero length.
- Added `tests/unit/code_scan_test.cpp`, covering an anchor inside a function, an anchor on a prologue, an exceeded search range, an empty result, and null input.
- After the reference scan, the function start and code region of five anchors are recorded as `null_context_code_region` events.
- When an anchor falls outside the `0xc0`-byte function-start window, an anchor-centered window (`0x20` before, `0x10` after) is recorded as well.
- Windows are cut from the already-read `.text` copy, so there are no additional remote reads.

## Verification evidence

- Full Windows x86 Debug build: passed.
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1219, failures: 0` (10 more than the 1209 at Task 158).
- Real-CHD runs: `20260903-134824-522.jsonl` and, after adding the anchor windows, `20260903-135023-263.jsonl`, both with `--diagnostic-idle-timeout 60000`.

### Function starts

| anchor | RVA | function start RVA | distance |
| --- | --- | --- | --- |
| field_read | `0x0001a699` | `0x0001a649` | 80 |
| write_candidate_0 | `0x0000fdbd` | `0x0000fc57` | 358 |
| write_candidate_1 | `0x0000fde1` | `0x0000fc57` | 394 |
| vtable_store_0 | `0x00010381` | `0x00010366` | 27 |
| vtable_store_1 | `0x000104a1` | `0x00010479` | 40 |

The two write candidates lie in the **same function** (`0x0000fc57`).

### Field-read function body

```
0041a649  55                    push ebp
0041a64a  8b ec                 mov  ebp, esp
0041a64c  81 ec 20 01 00 00     sub  esp, 0x120          <- boundary breakpoint
          56 57 51              push esi, edi, ecx
          8d bd e0 fe ff ff     lea  edi, [ebp-0x120]
          b9 48 00 00 00        mov  ecx, 0x48
          b8 cc cc cc cc        mov  eax, 0xcccccccc
          f3 ab                 rep  stosd               <- debug-build stack fill
          59                    pop  ecx
          89 8d e8 fe ff ff     mov  [ebp-0x118], ecx    <- store this
          83 7d 10 00           cmp  dword ptr [ebp+0x10], 0
          74 0b                 je   +0x0b
          8b 45 10              mov  eax, [ebp+0x10]
          89 85 e4 fe ff ff     mov  [ebp-0x11c], eax
          eb 09                 jmp  +0x09
          8b 4d 08              mov  ecx, [ebp+0x08]
          89 8d e4 fe ff ff     mov  [ebp-0x11c], ecx
          8d 55 f4              lea  edx, [ebp-0x0c]
          52                    push edx
          8b 85 e4 fe ff ff     mov  eax, [ebp-0x11c]
          50                    push eax
          8b 8d e8 fe ff ff     mov  ecx, [ebp-0x118]    <- this
0041a699  8b 89 1c 01 00 00     mov  ecx, [ecx+0x11c]    <- field read
          e8 f4 7b fe ff        call 0x00402298
          85 c0                 test eax, eax
          74 12                 je   +0x12
```

The function's only branch selects between arguments; nothing tests the field.

### Write-candidate function body

The start of function `0x0000fc57` does the following.

```
          a1 98 cd 94 00        mov  eax, [0x0094cd98]
          83 c0 01              add  eax, 1
          a3 98 cd 94 00        mov  [0x0094cd98], eax    <- counter increment
          8b 4d 14              mov  ecx, [ebp+0x14]
          89 4d fc              mov  [ebp-0x04], ecx      <- template source
          8b 15 9c cd 94 00     mov  edx, [0x0094cd9c]
          69 d2 d0 04 00 00     imul edx, edx, 0x4d0      <- stride
          81 c2 50 6d 94 00     add  edx, 0x00946d50      <- array base
          89 55 f4              mov  [ebp-0x0c], edx      <- new element
          68 d0 04 00 00        push 0x4d0
          6a 00                 push 0
          8b 45 f4 50           push [ebp-0x0c]
          e8 84 26 0b 00        call memset
```

It then writes a flag to `[element+0x118]`, copies `0xec` bytes from the template into `element+0x2c`, copies `+0x4c8`, and copies `0x17c` bytes from `+0x120` with `rep movsd`.

The two candidates are the two sides of an if/else.

```
0040fdbd  8b 45 f4              mov  eax, [ebp-0x0c]
          05 ac 04 00 00        add  eax, 0x4ac
          8b 4d f4              mov  ecx, [ebp-0x0c]
          89 81 1c 01 00 00     mov  [ecx+0x11c], eax     <- candidate 0: interior pointer
          ...                   push 0x27, template, element
          ff 15 24 17 ad 00     call dword ptr [0x00ad1724]
          eb 26                 jmp  +0x26
0040fde1  8b 4d f4              mov  ecx, [ebp-0x0c]
          c7 81 1c 01 00 00 00 00 00 00   mov dword ptr [ecx+0x11c], 0   <- candidate 1
```

### Vtable installation function

```
00010366  55 8b ec              push ebp; mov ebp, esp
          51                    push ecx
          c7 45 fc cc cc cc cc  mov  [ebp-0x04], 0xcccccccc
          89 4d fc              mov  [ebp-0x04], ecx      <- this
          8b 4d fc              mov  ecx, [ebp-0x04]
          e8 df 15 ff ff        call <base constructor>
          8b 45 fc              mov  eax, [ebp-0x04]
00010381  c7 00 54 d0 4d 00     mov  dword ptr [eax], 0x004dd054   <- install vtable
          8b 4d fc              mov  ecx, [ebp-0x04]
          c7 41 04 00 00 00 00  mov  dword ptr [ecx+0x04], 0
```

## Classification

* **Confirmed — there is no check before the field read.** The function's only branch selects an argument, and `[this+0x11c]` becomes the receiver of `call 0x00402298` as soon as it is loaded.
* **Confirmed — both write candidates belong to function `0x0000fc57`.** The function-start search resolved both anchors to the same prologue.
* **Confirmed — that function initializes static array elements.** It computes the element address with `imul edx, edx, 0x4d0` and `add edx, 0x00946d50`, then memsets `0x4d0` bytes. This is the basis for the Task 154 receivers `0x00946d50`, `0x00947220`, `0x009476f0`, `0x00947bc0`, and `0x00948090` and their `0x4d0` spacing.
* **Confirmed — the singleton's `+0x48` is element index 2 of that array.** `0x00946d50 + 2 * 0x4d0 = 0x009476f0`, matching the value Task 156 recorded.
* **Confirmed — the candidates fill the element's `+0x11c` on both sides of an if/else.** One writes `element + 0x4ac`, the other zero, and both branches then make an indirect call through `0x00ad1724`.
* **Confirmed — the vtable installation function has constructor shape.** It calls a base constructor, installs the vtable, and zeroes `+0x04`.
* **Inferred — `0x00ad1724` is an IAT slot.** The original image's `.idata` is RVA `0x006d1000` with size `0x0000171c`, and this address (RVA `0x006d1724`) falls inside it. Which import it is was not determined.
* **Classification — the code explains why the four candidates never write the singleton.** Candidates 0 and 1 are element-initialization code for a different class, whose receiver is structurally an element of that array. The singleton's `+0x11c` therefore has no dedicated writer in `.text`, and for it to be filled, candidate 2 or 3 (`0x0001825f`, `0x0001dbd3`) would have to execute with the singleton as receiver. Neither ran even once in the observed window.
* **Unresolved — the functions containing candidates 2 and 3 and their call conditions.** These are the next investigation. Direct field injection and Hardlock-response changes remain deferred.

## Next steps

1. Collect the function starts and bodies of the non-executing write candidates `0x0001825f` and `0x0001dbd3` the same way.
2. Determine which of the 40 singleton entry points call those functions.
3. If needed, resolve the import name at IAT slot `0x00ad1724` to understand the external dependency of the element-initialization path.

No original CHD/HDD/EXE content or Hardlock secret material was recorded in this document or the repository. Code quotations are limited to the instruction decoding the classification needs.
