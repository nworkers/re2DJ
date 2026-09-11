# EZ2Dancer I/O 포트 맵

## 한국어

### 범위와 상태

이 문서는 **EZ2Dancer** cabinet I/O를 다룹니다. EZ2DJ와는 별개의 보드이며, EZ2DJ 쪽 관찰은 [EZ2DJ I/O 포트 맵](ez2dj-io-map.md)에 있습니다.

이 문서의 본문 서술은 대부분 **추정**입니다. 근거가 독립된 공개 구현 한 곳이기 때문입니다. 다만 접근 폭과 출력 port `0x30a` 하나는 2026-09-10에 원본 실행 파일에서 확인됐습니다. 맨 아래 [원본 실행 파일에서의 첫 확인](#2026-09-10-원본-실행-파일에서의-첫-확인--first-confirmation-from-the-original-executable) 절을 함께 보십시오.

### 라이선스 취급

근거 자료는 [2EZConfig-V2 `ez2dancer-io`](https://github.com/ben-rnd/2EZConfig-V2/tree/master/src/2ez-dll/ez2dancer-io) (읽은 시점 커밋 `342beaab63c758f7ce39336441fcf8041d3a5360`)입니다. 이 저장소는 GNU General Public License v3입니다. re2DJ의 기본 라이선스는 BSD 3-Clause이고 AGENTS.md는 전염성 라이선스 코드의 도입을 금지하므로, **코드를 복사하거나 링크하지 않습니다.** 여기 기록하는 것은 외부에서 관찰 가능한 protocol, 즉 어떤 port를 어떤 폭으로 읽고 쓰며 각 bit가 무엇을 뜻하는지에 한합니다. 구현이 필요해지면 이 protocol 서술만 보고 독립적으로 작성합니다. 같은 방침을 EZ2DJ 쪽에서도 적용했습니다.

### 추정 — 접근 폭이 EZ2DJ와 다릅니다

가장 중요한 차이입니다.

| | EZ2DJ | EZ2Dancer |
| --- | --- | --- |
| 입력 명령 | `IN AL,DX` (`0xec`) | `IN AX,DX` (`0xed`) |
| 출력 명령 | `OUT DX,AL` (`0xee`) | `OUT DX,AX` (`0xef`) |
| 폭 | 8비트 | 16비트 |
| operand-size prefix | 없음 | `0x66`이 붙을 수 있음 |
| 명령 길이 | 1바이트 | prefix 없으면 1, 있으면 2 |
| port 범위 | `0x100`~`0x106` | `0x300`~`0x30c` |

공개 구현의 fault handler는 `EXCEPTION_PRIV_INSTRUCTION`에서 opcode를 읽고, `0x66`이면 다음 바이트를 opcode로 삼아 명령 길이를 2로 잡습니다. 그런 다음 `0xed`와 `0xef`만 처리하고 나머지는 통과시킵니다. 읽기 결과는 `EAX`의 하위 16비트에만 넣습니다.

### 추정 — 입력 port

| Port | 뜻 |
| --- | --- |
| `0x300` | P1 발판. 하위 12비트가 세 구역: `0x00f` 왼쪽, `0x0f0` 가운데, `0xf00` 오른쪽 |
| `0x302` | P2 발판. `0x300`과 같은 구역 배치 |
| `0x304` | 용도 미상. 구현은 값을 그대로 돌려주기만 함 |
| `0x306` | 손 센서와 TEST·SERVICE |

`0x300`과 `0x302`는 눌린 구역의 4비트 묶음을 지우는 방식으로 만들어진 뒤 전체가 반전됩니다. 즉 게임이 보는 값에서 눌림은 1로 나타납니다.

`0x306`의 상위 바이트는 손 센서 8개입니다.

| 비트 | 센서 |
| --- | --- |
| `0x0100` | P2 오른쪽 위 |
| `0x0200` | P2 왼쪽 위 |
| `0x0400` | P1 오른쪽 위 |
| `0x0800` | P1 왼쪽 위 |
| `0x1000` | P1 왼쪽 아래 |
| `0x2000` | P1 오른쪽 아래 |
| `0x4000` | P2 왼쪽 아래 |
| `0x8000` | P2 오른쪽 아래 |

하위 바이트에서 TEST는 비트 5, SERVICE는 비트 4이고 둘 다 active-low입니다. 최종 값은 상위 바이트만 `0xff00`으로 XOR합니다. 즉 **센서는 하위 바이트와 반대 극성으로 보고됩니다.** 공개 구현도 이 반전에 "game expects inverted sensors for some reason"이라는 주석만 달아 두었고 이유는 설명하지 않습니다. 따라서 이 반전이 보드 배선인지 게임 쪽 관례인지는 **미확정**입니다.

기본값은 `0x300`과 `0x302`가 `0xf000`, `0x304`가 `0x0000`, `0x306`이 `0x00ff`입니다.

### 추정 — 출력 port

| Port | 뜻 |
| --- | --- |
| `0x308` | 발판 관련 출력. 의미 미상 |
| `0x30a` | 캐비닛 조명. 16비트, active-low |
| `0x30c` | 손 센서 LED. 의미 미상 |

`0x30a`의 비트 배치입니다. 값 비트가 0이면 켜짐입니다.

| 비트 | 조명 |
| --- | --- |
| `0x0001` (b0) | 오른쪽 중간 |
| `0x0002` (b1) | 오른쪽 아래 |
| `0x0004` (b2) | 네온 |
| `0x0100` (b8) | 왼쪽 위 |
| `0x0200` (b9) | 왼쪽 중간 |
| `0x0400` (b10) | 왼쪽 아래 |
| `0x0800` (b11) | 오른쪽 위 |

`0x308`과 `0x30c`는 공개 구현에서도 해석되지 않습니다. 소스 주석이 "Requires further research"라고 적고 있습니다.

### 미확정

- ~~**원본 실행 파일에서의 확인.**~~ 출력 helper는 2026-09-10에 RVA `0x0000b565`로 확인됐습니다. 입력 helper는 여전히 미확정입니다.
- `0x304` 입력, `0x308`·`0x30c` 출력의 의미.
- 센서 상위 바이트 반전의 근거.
- 실제 cabinet 배선과 각 bit의 물리 순서.
- coin 입력 경로. EZ2DJ의 `0x105`에 대응하는 것이 여기 무엇인지는 이 자료에 없습니다.

### re2DJ 현재 상태에 대한 함의

`LegacyIoPortBus`는 byte 폭이고 `0x100`~`0x106`만 처리합니다. injected runtime의 privileged fault handler도 `0xec`/`0xee`만 인식하고 `EIP`를 항상 1 증가시킵니다. 두 가지 모두 EZ2Dancer에는 맞지 않으므로, `ez2d2m` 프로파일은 `legacy_io_ports`를 끈 채로 등록되어 있습니다. 켜면 `0x300` 읽기에 잘못된 값을 주거나 명령 중간으로 복귀하게 됩니다.

16비트 폭 지원은 [ez2d2m target 추가와 CHD 재귀 추출 설계](../design/20260910-239-ez2d2m-target-and-chd-extract.md)의 제외 범위이며 별도 작업으로 남아 있습니다.

## English

### Scope and status

This document covers **EZ2Dancer** cabinet I/O. It is a different board from EZ2DJ's, whose observations live in [the EZ2DJ I/O port map](ez2dj-io-map.md).

The body of this document is mostly **inferred**, resting on one independent public implementation. The access width and the single output port `0x30a` were confirmed against the original executable on 2026-09-10; see the final section, First confirmation from the original executable.

### Licence handling

The source of these facts is [2EZConfig-V2 `ez2dancer-io`](https://github.com/ben-rnd/2EZConfig-V2/tree/master/src/2ez-dll/ez2dancer-io), read at commit `342beaab63c758f7ce39336441fcf8041d3a5360`. That repository is under the GNU General Public License v3. re2DJ's baseline licence is BSD 3-Clause and AGENTS.md forbids introducing copyleft code, so **none of it is copied or linked.** What is recorded here is the externally observable protocol only — which ports are read and written, at what width, and what each bit means. Any implementation is written independently from this protocol description. The same policy was applied on the EZ2DJ side.

### Inferred — the access width differs from EZ2DJ

This is the most important difference. EZ2DJ uses byte-wide access, `IN AL,DX` (`0xec`) and `OUT DX,AL` (`0xee`), over ports `0x100` to `0x106`, with a one-byte instruction. EZ2Dancer uses word-wide access, `IN AX,DX` (`0xed`) and `OUT DX,AX` (`0xef`), over ports `0x300` to `0x30c`, and the instruction may carry a `0x66` operand-size prefix, making it two bytes.

The public implementation's fault handler reads the opcode at the faulting address, treats the next byte as the opcode and the length as two when it finds `0x66`, then handles only `0xed` and `0xef` and passes everything else through. A read result is placed into the low 16 bits of `EAX` only.

### Inferred — input ports

`0x300` is the P1 dance pad and `0x302` the P2 pad, each using the low 12 bits as three zones: `0x00f` left, `0x0f0` centre, `0xf00` right. Both are built by clearing the four-bit group of a pressed zone and then inverting the whole value, so a press reads as ones in what the game sees.

`0x304` has no known use; the implementation returns a cached value unchanged.

`0x306` carries the hand sensors in its high byte — `0x0100` P2 upper right, `0x0200` P2 upper left, `0x0400` P1 upper right, `0x0800` P1 upper left, `0x1000` P1 lower left, `0x2000` P1 lower right, `0x4000` P2 lower left, `0x8000` P2 lower right — and TEST at bit 5 and SERVICE at bit 4 of its low byte, both active-low. The final value XORs the high byte with `0xff00`, so **the sensors are reported at the opposite polarity from the low byte.** The public implementation notes only that "game expects inverted sensors for some reason" without explaining it, so whether the inversion is board wiring or a game-side convention is **unresolved**.

The idle values are `0xf000` for `0x300` and `0x302`, `0x0000` for `0x304`, and `0x00ff` for `0x306`.

### Inferred — output ports

`0x30a` drives the cabinet lights as a 16-bit active-low word: bit 0 right middle, bit 1 right bottom, bit 2 neon, bit 8 left top, bit 9 left middle, bit 10 left bottom, bit 11 right top. A zero bit means lit.

`0x308` is pad-related output and `0x30c` drives the hand-sensor LEDs, but neither is interpreted by the public implementation, whose own comment says they require further research.

### Unresolved

- **Confirmation against the original executable.** The raw-I/O helper RVAs in `ez2dancer/EZ2Dancer.exe` are unknown; obtaining them requires running it and reading the privileged fault's address. As the EZ2DJ profiles already showed, values carried over from another build do not match.
- The meaning of input `0x304` and outputs `0x308` and `0x30c`.
- The reason for the sensor high-byte inversion.
- The real cabinet wiring and the physical order of each bit.
- The coin input path. This material says nothing about an EZ2Dancer equivalent of EZ2DJ's `0x105`.

### What this means for re2DJ today

`LegacyIoPortBus` is byte-wide and serves only `0x100` through `0x106`, and the injected runtime's privileged-fault handler recognises only `0xec` and `0xee` and always advances `EIP` by one. Neither fits EZ2Dancer, so the `ez2d2m` profile is registered with `legacy_io_ports` off: turning it on would answer a `0x300` read with a wrong value or resume inside an instruction.

Word-width support is out of scope in [the ez2d2m target and recursive CHD extraction design](../design/20260910-239-ez2d2m-target-and-chd-extract.md) and remains separate work.

---

## 2026-09-10 원본 실행 파일에서의 첫 확인 / First confirmation from the original executable

### 한국어

이 문서는 작성 시점에 서술 전체가 **추정**이었습니다. 근거가 공개 구현 하나뿐이었기 때문입니다. `ez2d2m`이 Hardlock 보호를 지나 원본 `.text`에서 실행되면서 일부가 원본 바이너리에서 **확인됨**으로 올라갔습니다.

경위는 [ez2d2m 후보 map 재판별](ez2d2m-chd-filesystem.md)에 있습니다. 요약하면, `candidate-70` map으로 실행한 게스트가 원본 `.text`의 트랩되지 않은 privileged instruction에서 멈췄고, 그 지점의 code window를 디코드했습니다. 두 번 실행에서 동일합니다.

#### 확인됨 — 접근 폭

| 항목 | 관측값 |
| --- | --- |
| 명령 바이트 | `66 ef` |
| 명령 | `OUT DX, AX` |
| 명령 길이 | 2바이트 (`0x66` operand-size prefix 포함) |
| fault RVA | `0x0000b565` (`.text` 안, main image `0x0040b565`) |
| 앞 명령 | `66 8b c7` = `mov ax, di` |

**EZ2Dancer는 16비트 폭 port 접근을 사용합니다.** 이는 더 이상 추정이 아닙니다. EZ2DJ의 byte 폭 `ee`/`ec`와 다르며, `0x66` prefix가 실제로 붙는다는 점도 확인됐습니다. 따라서 명령 길이는 2바이트이고, fault handler가 `EIP`를 1만 증가시키면 명령 중간으로 복귀합니다.

#### 확인됨 — 출력 port 하나

| 항목 | 관측값 |
| --- | --- |
| `edx` | `0x030a` |
| `eax` | `0x00000004` |

**port `0x30a`가 출력 대상임이 확인됐습니다.** 이 문서가 추정으로 기록한 캐비닛 조명 port와 같은 주소입니다. 기록된 값 `0x0004`는 추정 표의 `NEON` bit와 같습니다.

#### 여전히 미확정

- **나머지 port 전부.** 입력 `0x300`·`0x302`·`0x304`·`0x306`과 출력 `0x308`·`0x30c`는 이번 실행이 도달하기 전에 멈췄으므로 관측되지 않았습니다.
- **bit 배치 전체.** `0x30a`에 대한 관측은 표본 한 개입니다. `0x0004`가 `NEON`과 일치하는 것은 추정 표와 모순되지 않는다는 뜻이지, bit 배치를 확정하지 않습니다. active-low 여부도 이 한 값으로는 정해지지 않습니다.
- **입력 helper 위치.** 이 실행은 출력에서 먼저 멈췄습니다. `IN AX,DX`(`66 ed`) 위치는 16비트 경계가 생겨 이 출력을 트랩한 뒤에야 관측됩니다.
- 센서 상위 바이트 반전, coin 입력 경로, 실제 배선.

#### 프로파일에 대한 함의

`ez2d2m` 프로파일의 `legacy_io_ports`는 계속 꺼 둡니다. 확인된 helper 주소 `0x0000b565`를 지금의 `legacy_io_in_byte_rva`/`legacy_io_out_byte_rva`에 넣는 것은 **잘못입니다.** 그 경로는 byte 폭 의미와 1바이트 명령을 전제하므로, 16비트 명령에 byte 값을 주고 `EIP`를 잘못 진행시킵니다. 이 주소는 16비트 경계가 생길 때 쓰기 위해 여기에 기록만 합니다.

### English

Every statement in this document was **inferred** when it was written, resting on a single public implementation. Parts of it are now **confirmed** against the original binary, because `ez2d2m` got past its Hardlock protection and executed original `.text`.

The circumstances are in [the ez2d2m candidate re-judgement](ez2d2m-chd-filesystem.md). In short, a guest running the `candidate-70` map stopped on an untrapped privileged instruction inside the original `.text`, and the code window at that point was decoded — identically across two runs.

#### Confirmed — the access width

The faulting bytes are `66 ef`, an `OUT DX, AX` two bytes long including its `0x66` operand-size prefix, at fault RVA `0x0000b565` (main image `0x0040b565`), preceded by `66 8b c7` (`mov ax, di`).

**EZ2Dancer uses 16-bit-wide port access.** This is no longer an inference. It differs from EZ2DJ's byte-wide `ee` / `ec`, and the `0x66` prefix is confirmed to be present in practice, which makes the instruction two bytes long: a fault handler that advances `EIP` by one would resume inside it.

#### Confirmed — one output port

The fault carries `edx` = `0x030a` and `eax` = `0x00000004`. **Port `0x30a` is confirmed as an output target**, the same address this document recorded by inference as the cabinet-lights port, and the written value `0x0004` is the inferred table's `NEON` bit.

#### Still unresolved

Every other port: inputs `0x300`, `0x302`, `0x304` and `0x306` and outputs `0x308` and `0x30c` were not reached before the run stopped. The full bit layout: one sample for `0x30a` means `0x0004` is consistent with the inferred table, not that the layout or its active-low sense is established. The input helper's location: this run stopped on output first, so `IN AX,DX` (`66 ed`) can only be observed once a 16-bit boundary traps this output. And the sensor high-byte inversion, the coin input path and the real wiring.

#### What this means for the profile

`legacy_io_ports` stays off for the `ez2d2m` profile. Putting the confirmed helper address `0x0000b565` into today's `legacy_io_in_byte_rva` or `legacy_io_out_byte_rva` would be **wrong**: that path assumes byte-width semantics and a one-byte instruction, so it would hand a byte value to a 16-bit instruction and advance `EIP` incorrectly. The address is recorded here for use when a 16-bit boundary exists.

---

## 2026-09-10 word 경계 가동 후의 관측 / Observations once the word boundary ran

### 한국어

[16비트 폭 legacy I/O 경계](../design/20260910-244-word-width-legacy-io.md)를 구현하고 `ez2d2m`을 실행해 얻은 결과입니다. 경계가 port 접근을 기록하므로, 이제 게스트가 실제로 무엇을 건드리는지 볼 수 있습니다.

#### 확인됨 — 트랩이 성립합니다

RVA `0x0000b565`의 privileged instruction fault가 사라지고 실행이 이어집니다. 게스트는 crash 대신 `.protect`의 RVA `0x0043843a`에서 `ExitProcess(0)`으로 종료합니다. Hardlock 요청도 31건(initialize 1, handshake 4, descriptor 15, transform 11)으로 늘었습니다.

#### 확인됨 — 게스트가 실제로 건드리는 port

한 실행에서 관측된 port 접근 전부입니다.

| 방향 | 폭 | port | 횟수 |
| --- | --- | --- | --- |
| write | 16 | `0x030a` | 8 |

**읽기는 한 번도 없습니다.** 종료 전까지 게스트는 입력 port를 전혀 읽지 않으므로, 입력 helper의 주소는 여전히 관측되지 않았습니다. 프로파일의 `legacy_io_in_rva`가 0으로 남는 이유입니다.

`0x308`과 `0x30c`도 이 실행에서는 쓰이지 않았습니다. 두 port의 의미는 계속 미확정입니다.

#### 확인됨 — `0x30a`의 bit 집합

여덟 번의 쓰기는 값이 누적되는 순서열입니다.

| 순서 | 값 | 새로 켜진 bit |
| --- | --- | --- |
| 1 | `0x0004` | b2 |
| 2 | `0x0104` | b8 |
| 3 | `0x0304` | b9 |
| 4 | `0x0704` | b10 |
| 5 | `0x0f04` | b11 |
| 6 | `0x0f05` | b0 |
| 7 | `0x0f07` | b1 |
| 8 | `0x0f07` | (없음) |

최종값 `0x0f07`은 bit 0, 1, 2, 8, 9, 10, 11입니다. **이는 이 문서가 추정으로 기록한 캐비닛 조명 7개의 bit 집합과 정확히 일치하며, 그 밖의 bit는 하나도 쓰이지 않습니다.** 공개 구현과 무관하게 원본 자신이 이 7개 위치만 사용한다는 것이 확인됐습니다.

#### 미확정 — 극성

이 순서열은 **극성을 정하지 않습니다.** 한 번에 하나씩 bit를 세우는 동작은 "램프를 하나씩 켠다"(active-high)로도, "모두 켜진 상태에서 하나씩 끈다"(active-low)로도 읽힙니다. 구현은 공개 구현을 따라 active-low로 두었지만, 이 관측은 그것을 뒷받침하지도 반박하지도 않습니다.

각 bit가 **어느** 램프인지도 미확정입니다. 확인된 것은 7개 위치의 집합이지 그 배정이 아닙니다.

### English

These are the results of implementing [the word-width legacy I/O boundary](../design/20260910-244-word-width-legacy-io.md) and running `ez2d2m`. The boundary records port accesses, so what the guest really touches is now visible.

#### Confirmed — the trap holds

The privileged-instruction fault at RVA `0x0000b565` is gone and execution continues. Instead of crashing, the guest ends by calling `ExitProcess(0)` from RVA `0x0043843a` inside `.protect`, and its Hardlock traffic grows to 31 requests: 1 initialize, 4 handshakes, 15 descriptors and 11 transforms.

#### Confirmed — which ports the guest actually touches

Every port access observed in a run is eight 16-bit writes to `0x030a`, and nothing else.

**There is not one read.** The guest never reads an input port before it exits, so the input helper's address remains unobserved — which is why the profile's `legacy_io_in_rva` stays zero. `0x308` and `0x30c` were not written either, so both remain unresolved.

#### Confirmed — the bit set of `0x030a`

The eight writes are a cumulative sequence: `0x0004`, `0x0104`, `0x0304`, `0x0704`, `0x0f04`, `0x0f05`, `0x0f07`, `0x0f07` — setting bits 2, 8, 9, 10, 11, 0 and 1 in that order and then repeating the final value.

The end state `0x0f07` is exactly bits 0, 1, 2, 8, 9, 10 and 11. **That is precisely the seven-bit cabinet-light set this document recorded by inference, and no other bit is ever written.** The original itself therefore confirms that these seven positions are the ones in use, independently of the public implementation.

#### Unresolved — the polarity

The sequence does **not** settle polarity. Setting one bit at a time reads equally as "light one lamp after another" (active high) or "start from all lit and switch them off one by one" (active low). The implementation follows the public description and treats them as active low, but this observation neither supports nor contradicts that.

Which lamp each bit drives is likewise unresolved: the set of seven positions is confirmed, not their assignment.
