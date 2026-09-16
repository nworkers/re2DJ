# 작업 293 설계 — legacy I/O helper 시그니처 탐색 / Task 293 design — Locating legacy I/O helpers by signature

선행: [작업 292 복호화된 주 이미지 덤프](20260916-292-decrypted-image-dump.md)

## 한국어

### 문제

프로파일마다 `legacy_io_in_rva`와 `legacy_io_out_rva`를 갖는다. 게스트가 트랩되지 않은 `in`/`out`으로 privileged fault를 내면 런타임이 이 주소로 helper를 식별한다. 값이 틀리면 fault가 처리되지 않고 실행이 `0xc0000096`에서 멈춘다.

문제는 **여러 프로파일의 값이 그 빌드에서 확인된 것이 아니라는 점**이다. 1st SE는 추출된 `.gtide` 빌드에서 읽은 값을 CHD `.protect` 빌드에 쓰고 있고, 5th는 4th 값을 물려받았다. [1st·5th Hardlock descriptor 분석](../analysis/ez2dj1st-5th-hardlock-descriptors.md)은 두 빌드가 transform loop를 넘긴 뒤 정확히 `0xc0000096`에서 멈춘다고 기록한다. 즉 **응답이 틀린 것이 아니라 I/O 경계가 준비되지 않은 것**이다.

지금까지 이 값을 얻는 방법은 실행해서 fault 주소를 읽는 것뿐이었다. 보호 빌드는 `.text`가 디스크에서 암호문이라 정적으로 찾을 수 없었기 때문이다.

### 이번에 확인한 것 — 확인됨 (2026-09-16)

1st SE 정식 빌드(`.gtide`, 디스크 평문 `.text`)의 프로파일 값 주변을 직접 읽었다.

```
RVA 0x00038980:  33 c0 66 8b 54 24 04 ec c3
                 xor eax,eax; mov dx,[esp+4]; in al,dx; ret      -> 0x38987 = legacy_io_in_rva
RVA 0x000389a2:  66 8b 54 24 04 8a 44 24 08 ee c3
                 mov dx,[esp+4]; mov al,[esp+8]; out dx,al; ret  -> 0x389ab = legacy_io_out_rva
```

두 값이 프로파일과 **정확히 일치**한다. 그리고 두 helper는 고립되어 있지 않고 폭별 변형이 연속으로 배치된 한 묶음이다.

| helper | 시그니처 | opcode 위치 | 확인된 RVA |
| --- | --- | --- | --- |
| `inportb` | `33 C0 66 8B 54 24 04 EC C3` | +7 | `0x00038987` |
| `inportw` | `66 8B 54 24 04 66 ED C3` | +5 | `0x0003898e` |
| `inportl` | `66 8B 54 24 04 ED C3` | +5 | `0x00038996` |
| `outportb` | `66 8B 54 24 04 8A 44 24 08 EE C3` | +9 | `0x000389ab` |
| `outportw` | `66 8B 54 24 04 66 8B 44 24 08 66 EF C3` | +10 | `0x000389b7` |

이것은 컴파일러 런타임의 `inp`/`outp` 계열 helper이며 빌드마다 위치만 다르고 형태는 같다. 따라서 **시그니처로 찾을 수 있다.**

### 설계

`re2dj::exe::code_scan`에 시그니처 탐색을 추가한다. 이 모듈은 이미 "구문적 탐색이며 결과는 후보"라는 계약을 갖고 있고 플랫폼 중립이므로 그대로 맞는다.

```
ScanPortHelpers(bytes, size, base_address, max_sites, capped, total_sites)
  -> std::vector<PortHelperSite>{ kind, signature_offset, opcode_address }
```

입력은 두 가지 중 하나다.

* **평문 `.text`를 가진 빌드** — 디스크 파일을 그대로 넣는다. 이 이미지들은 `.text`의 raw offset과 virtual address가 같으므로 파일 오프셋이 곧 RVA다.
* **보호 빌드** — 작업 292의 `resumed` 덤프를 넣는다. 덤프는 virtual 레이아웃이라 역시 파일 오프셋이 곧 RVA다.

두 경우 모두 base address를 인자로 받아 호출자가 RVA와 VA 중 원하는 것을 얻게 한다.

CLI는 `src/tools/`에 독립 도구로 둔다. `code_score`와 같은 성격, 곧 원본 자산을 읽어 사실만 보고하고 아무것도 바꾸지 않는 오프라인 도구다.

### 왜 런처에 넣지 않는가

런타임 fault 주소 관찰은 이미 런처가 한다. 이 작업이 더하는 것은 **실행 전에 값을 얻는 경로**이므로 실행 경로에 둘 이유가 없다. 오프라인 도구로 두면 덤프 파일 하나만 있으면 되고, 게임을 띄우지 않고도 프로파일 값을 검증할 수 있다.

### 한계 — 설계에 명시한다

* 시그니처 탐색은 **구문적**이다. 같은 바이트 열이 다른 명령 안이나 코드에 박힌 데이터에 나타날 수 있으므로 결과는 후보다. `code_scan`의 기존 주석과 같은 계약을 따른다.
* 시그니처는 **1st SE 한 빌드에서 확인한 형태**다. 다른 컴파일러나 최적화 수준에서는 다를 수 있다. 맞지 않는 빌드가 나오면 그 빌드의 실제 바이트를 근거로 시그니처를 추가하고, 추정으로 일반화하지 않는다.
* 후보가 프로파일 값과 일치한다고 해서 그 값이 **게임이 실제로 부르는 helper**임을 증명하지는 않는다. 그것은 여전히 런타임 fault 관찰이 확정한다. 이 도구는 후보를 좁히고 명백한 불일치를 잡아낸다.

### 대안과 기각 이유

**대안 1 — 각 빌드를 실행해 fault 주소를 읽는다.** 현재 방법이며 확정력은 가장 높다. 그러나 1st SE와 5th는 그 지점까지 가지 못해 순환에 빠진다. 값이 틀려서 멈추는데 값을 얻으려면 그 지점을 지나야 한다.

**대안 2 — 덤프를 역어셈블한다.** 도구 의존이 커지고 이 목적에는 과하다. 찾는 대상이 고정된 짧은 바이트 열이다.

## English

Prerequisite: [Task 292, decrypted main-image dump](20260916-292-decrypted-image-dump.md)

### Problem

Each profile carries a `legacy_io_in_rva` and a `legacy_io_out_rva`. When the guest raises a privileged fault on an untrapped `in`/`out`, the runtime identifies the helper by these addresses; wrong values leave the fault unhandled and execution stops at `0xc0000096`.

The difficulty is that **several profiles carry values never confirmed against their own build**. 1st SE uses values read from the extracted `.gtide` build while running the CHD `.protect` build, and 5th inherited 4th's. The [1st and 5th Hardlock descriptor analysis](../analysis/ez2dj1st-5th-hardlock-descriptors.md) records both builds stopping at exactly `0xc0000096` past the transform loop, meaning **the response is not wrong; the I/O boundary is not prepared**.

Until now the only way to obtain these values was to run the build and read the faulting address, because a protected build's `.text` is ciphertext on disk and cannot be searched statically.

### What was confirmed — confirmed (2026-09-16)

Reading around the profile values in the 1st SE canonical build (`.gtide`, plaintext `.text` on disk):

```
RVA 0x00038980:  33 c0 66 8b 54 24 04 ec c3
                 xor eax,eax; mov dx,[esp+4]; in al,dx; ret      -> 0x38987 = legacy_io_in_rva
RVA 0x000389a2:  66 8b 54 24 04 8a 44 24 08 ee c3
                 mov dx,[esp+4]; mov al,[esp+8]; out dx,al; ret  -> 0x389ab = legacy_io_out_rva
```

Both match the profile **exactly**, and neither helper stands alone: the width variants sit together as one block.

| Helper | Signature | Opcode offset | Confirmed RVA |
| --- | --- | --- | --- |
| `inportb` | `33 C0 66 8B 54 24 04 EC C3` | +7 | `0x00038987` |
| `inportw` | `66 8B 54 24 04 66 ED C3` | +5 | `0x0003898e` |
| `inportl` | `66 8B 54 24 04 ED C3` | +5 | `0x00038996` |
| `outportb` | `66 8B 54 24 04 8A 44 24 08 EE C3` | +9 | `0x000389ab` |
| `outportw` | `66 8B 54 24 04 66 8B 44 24 08 66 EF C3` | +10 | `0x000389b7` |

These are the compiler runtime's `inp`/`outp` family: the same shape in every build, only at a different address. They can therefore **be found by signature**.

### Design

Add the signature search to `re2dj::exe::code_scan`, which already carries the "syntactic search, results are candidates" contract and is platform-neutral.

```
ScanPortHelpers(bytes, size, base_address, max_sites, capped, total_sites)
  -> std::vector<PortHelperSite>{ kind, signature_offset, opcode_address }
```

The input is one of two things. For a **build with plaintext `.text`** it is the disk file, whose `.text` raw offset equals its virtual address, so a file offset is an RVA. For a **protected build** it is task 292's `resumed` dump, which is in virtual layout, so a file offset is again an RVA. Both take a base address so the caller chooses RVA or VA.

The CLI goes in `src/tools/` as a standalone offline tool in the same spirit as `code_score`: it reads original assets, reports facts, and changes nothing.

### Why not in the launcher

The launcher already observes the faulting address at run time. What this task adds is a path to the value **before a run**, so it has no business on the execution path. Kept offline, it needs only a dump file and verifies a profile value without launching the game.

### Limits — stated in the design

* The search is **syntactic**: the same bytes can occur inside another instruction or in data embedded in code, so results are candidates, under the same contract as the rest of `code_scan`.
* The signatures are **the shape confirmed in one build**. Another compiler or optimization level may differ. When a build does not match, add a signature grounded in that build's actual bytes rather than generalizing by inference.
* A candidate agreeing with a profile value does not prove it is **the helper the game actually calls**; run-time fault observation still settles that. This tool narrows candidates and catches clear mismatches.

### Alternatives rejected

**Running each build to read the faulting address** is the current method and remains the most conclusive, but 1st SE and 5th never reach that point, which is circular: they stop because the value is wrong, and obtaining the value requires passing the point where they stop.

**Disassembling the dump** adds a heavy tool dependency for a target that is a short fixed byte sequence.
