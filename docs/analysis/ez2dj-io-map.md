# EZ2DJ I/O 포트 맵

## 한국어

### 확인됨 — 원본 1st SE 실행 파일

- byte 입력 helper는 `0x101`~`0x106`, byte 출력 helper는 `0x100`~`0x103`과 `0x106`을 사용한다.
- `0x101`, `0x102`, `0x106` 입력은 bitwise NOT 뒤 boolean 상태로 사용되므로 active-low다.
- `0x103`~`0x105`는 이전 값과 비교된다. `0x105`는 `current - previous`가 음수이면 256을 더한 delta를 credit 관련 전역에 누적하므로 8비트 증가 counter다.
- 근거와 주소는 [원본 실행 파일 구조 분석](ez2dj-exe-structures.md)과 [legacy port HLE 설계](../design/20260825-062-legacy-io-port-hle.md)에 누적되어 있다.

### 확인됨 — 실제 키보드 실행과 원본 counter 계산

사용자 실제 실행에서 F3에 연결한 기존 `0xfe → 0xff` pulse는 credit을 99까지 증가시켰다. 대응 unprotected binary의 VA `0x0041764c`는 port `0x105`의 `current - previous`를 계산하고 음수면 `0x100`을 더하며, VA `0x00417675`부터 그 delta를 credit 관련 누적값에 더한다. 따라서 `0x105`는 press마다 한 방향으로 1 증가하고 read 뒤에도 유지되는 modulo-256 counter여야 한다. 되돌아가는 pulse 모델은 잘못된 것으로 확인됐다.

작업 087은 초기값 `0x00`의 stable 8-bit counter를 구현하고 false→true마다 1 증가시키며 read는 값을 변경하지 않도록 정정했다. 공용 hold/release/repress/wrap test와 표준 Windows x86 build, CTest 3/3이 통과했다. 실제 press당 credit 1 증가는 사용자 재검증 전이므로 **미확정**이다.

### 추정 — 독립된 공개 구현과의 교차 확인

[2EZConfig-V2 commit `a7346e0`](https://github.com/ben-rnd/2EZConfig-V2/tree/a7346e066bb569643e0cb25f41cfdc079126fbc6)의 공개 구현은 같은 port 범위에 다음 의미를 부여한다. 이는 원본 cabinet 또는 회로도에서 직접 확인한 사실이 아니므로 프로젝트 분석에서는 **추정**으로 유지한다.

| 방향 | Port | 추정 의미 |
| --- | --- | --- |
| IN | `0x101` | P1/P2 Start, Effector 1~4, Service, Test(active-low) |
| IN | `0x102` | P1 key 1~5, pedal(active-low) |
| IN | `0x103` | P1 turntable absolute 8-bit position |
| IN | `0x104` | P2 turntable absolute 8-bit position |
| IN | `0x105` | coin 입력에 연결된 8비트 누적 counter; press마다 +1, read로 소비하지 않음 |
| IN | `0x106` | P2 key 1~5, pedal(active-low) |
| OUT | `0x100` | red/blue cabinet lamps, neon(active-high) |
| OUT | `0x101` | Start와 Effector lamps(active-high) |
| OUT | `0x102` | P1 key와 turntable lamps(active-high) |
| OUT | `0x103` | P2 key와 turntable lamps(active-high) |

구체적인 교차 확인 위치는 [입력 구현](https://github.com/ben-rnd/2EZConfig-V2/blob/a7346e066bb569643e0cb25f41cfdc079126fbc6/src/2ez-dll/ez2dj-io/ez2dj_io_input.cpp)과 [출력 구현](https://github.com/ben-rnd/2EZConfig-V2/blob/a7346e066bb569643e0cb25f41cfdc079126fbc6/src/2ez-dll/ez2dj-io/ez2dj_io_output.cpp)이다. 해당 저장소는 [GPL-3.0](https://github.com/ben-rnd/2EZConfig-V2/blob/a7346e066bb569643e0cb25f41cfdc079126fbc6/LICENSE)이므로 re2DJ는 코드를 복사·링크하지 않고 관찰 가능한 protocol만 독립 구현한다.

### 미확정

- 실제 1st SE cabinet 배선과 모든 bit의 물리 순서
- 원본 분석에서 관찰된 `OUT 0x106`의 의미
- turntable 초기 위치가 cabinet power-on 동작에서도 `0x80`인지 여부
- 각 출력 lamp의 실제 장치 연동과 timing 요구사항

## English

### Confirmed — original 1st SE executable

The byte helpers read ports `0x101` through `0x106` and write `0x100` through `0x103` plus `0x106`. Inputs `0x101`, `0x102`, and `0x106` are active-low. Values from `0x103` through `0x105` are compared with prior samples. For `0x105`, the original computes `current - previous`, adds 256 when negative, and accumulates that delta into credit-related globals, confirming an eight-bit increasing counter.

### Confirmed — real keyboard run and original counter calculation

In the user's real run, the old `0xfe` then `0xff` pulse mapped to F3 drove credit to 99. At VA `0x0041764c`, the corresponding unprotected binary computes port `0x105` current minus previous, adds `0x100` when negative, and from VA `0x00417675` accumulates the delta into credit-related globals. Port `0x105` must therefore increase once per press and remain stable after reads; the returning pulse model is confirmed incorrect.

Task 087 implements a stable eight-bit counter initialized to `0x00`, incremented once per false-to-true transition and unchanged by reads. Shared hold/release/repress/wrap coverage and the standard Windows x86 build plus CTest 3/3 pass. Exactly one real credit per press remains **unresolved** pending user revalidation.

### Inferred — cross-check against an independent public implementation

2EZConfig-V2 commit `a7346e0` assigns the semantic meanings listed in the table above to the same port range. Because this was not verified from original cabinet hardware or schematics, re2DJ records the physical mapping as **inferred**. Its GPL-3.0 code is neither copied nor linked; re2DJ independently implements only the externally observable protocol under its BSD-3-Clause policy.

### Unresolved

The exact 1st SE cabinet wiring, the meaning of `OUT 0x106`, hardware power-on turntable position, and physical lamp timing remain unresolved.

### 2026-09-06 2nd Trax helper addresses

**확인됨:** `ez2dj2nd` attached diagnostic `logs/windows_x86_launcher_probe/ez2dj2nd/20260906-022613-342.jsonl`에서 input helper는 VA `0x004782d7` (RVA `0x000782d7`, `IN AL,DX`)로, output helper는 VA `0x0047832b` (RVA `0x0007832b`, `OUT DX,AL`)로 확인됐다. 첫 fault의 port는 `0x0103`이고, output 관찰에는 `0x0100`~`0x0103`이 포함됐다. 후속 run은 두 helper를 현재 `LegacyIoPortBus`로 처리했으며 second-chance privileged fault가 없었다.

이 주소들은 2nd 실행 파일의 helper 위치에 대한 **확인됨** 값이다. port별 장치 의미는 1st SE에서 교차 확인된 표를 2nd에 자동으로 확정하는 근거가 없으므로 **미확정**으로 남긴다. `id_ref`/`id_verify`와 Hardlock 응답은 이 raw port 주소 관찰과 별개의 경계다.

*Confirmed: the attached 2nd diagnostic `logs/windows_x86_launcher_probe/ez2dj2nd/20260906-022613-342.jsonl` identifies the input helper at VA `0x004782d7` (RVA `0x000782d7`, `IN AL,DX`) and the output helper at VA `0x0047832b` (RVA `0x0007832b`, `OUT DX,AL`). The first fault used port `0x0103`, and output observations included `0x0100` through `0x0103`. A follow-up run handled both helpers through the current `LegacyIoPortBus` without a second-chance privileged fault.*

*These are **confirmed** helper locations in the 2nd executable. Per-port device meanings remain **unresolved**, because the 1st SE cross-check does not automatically establish the 2nd wiring. `id_ref`/`id_verify` and the Hardlock response are separate boundaries from these raw-port observations.*

### 2026-09-16 helper 시그니처와 프로파일 값의 확인 상태 / Helper signatures and the confirmation status of profile values

**확인됨.** 이 helper들은 고립된 함수가 아니라 컴파일러 런타임의 `inp`/`outp` 계열이 폭별로 연속 배치된 한 묶음이다. 1st SE 정식 빌드(`.gtide`, 디스크 평문 `.text`)에서 실제 바이트로 확인한 형태는 다음과 같다. opcode 위치는 명령의 **시작**이며, `66` operand-size 접두사가 있으면 그 접두사를 가리킨다. privileged fault가 보고하는 주소와 프로파일이 저장하는 값이 이것이다.

*Confirmed. These are not isolated functions but the compiler runtime's `inp`/`outp` family laid out together by width. The shapes below were read out of the 1st SE canonical build (`.gtide`, plaintext `.text` on disk). The opcode offset points at the **start** of the instruction, including a `66` operand-size prefix when present, which is both what a privileged fault reports and what a profile stores.*

| helper | 시그니처 / signature | opcode 위치 | 1st SE 확인 RVA |
| --- | --- | --- | --- |
| `inportb` | `33 C0 66 8B 54 24 04 EC C3` | +7 | `0x00038987` |
| `inportw` | `66 8B 54 24 04 66 ED C3` | +5 | `0x0003898e` |
| `inportl` | `66 8B 54 24 04 ED C3` | +5 | `0x00038996` |
| `outportb` | `66 8B 54 24 04 8A 44 24 08 EE C3` | +9 | `0x000389ab` |
| `outportw` | `66 8B 54 24 04 66 8B 44 24 08 66 EF C3` | +10 | `0x000389b7` |

`inportb`와 `outportb`의 값은 `ez2dj1stse` 프로파일의 `legacy_io_in_rva`·`legacy_io_out_rva`와 **정확히 일치**한다. 탐색은 `re2dj_port_helper_scan`으로 재현할 수 있다.

*The `inportb` and `outportb` values match the `ez2dj1stse` profile's `legacy_io_in_rva` and `legacy_io_out_rva` **exactly**. The search is reproducible with `re2dj_port_helper_scan`.*

**확인됨.** `.protect` 계열 보호 빌드의 디스크 파일에서는 이 시그니처가 하나도 나오지 않는다. 3rd, 4th, `ez2d2m` 모두 0건이다. `.text`가 디스크에서 암호문이라는 뜻이며 [보호 빌드의 런타임 복호화](protected-build-runtime-decryption.md)와 일치한다. 이 빌드들은 작업 292의 `resumed` 덤프를 대상으로 탐색해야 한다.

*Confirmed. None of these signatures appears in a `.protect`-family protected build's disk file — zero hits for 3rd, 4th and `ez2d2m` alike — meaning `.text` is ciphertext on disk, consistent with [runtime decryption in protected builds](protected-build-runtime-decryption.md). Those builds must be searched in task 292's `resumed` dump instead.*

**미확정 — 어느 프로파일 값이 그 빌드에서 확인되지 않았는가.** 값이 틀리면 privileged fault가 처리되지 않아 실행이 `0xc0000096`에서 멈춘다.

*Unresolved — which profile values were never confirmed against their own build. A wrong value leaves the privileged fault unhandled and stops execution at `0xc0000096`.*

| 프로파일 | 값의 출처 | 상태 |
| --- | --- | --- |
| `ez2dj2nd` | 실제 실행의 fault 주소 | **확인됨** |
| `ez2dj1stse` | 추출된 `.gtide` 빌드의 평문 `.text` | CHD `.protect` 빌드에서는 **미확정** |
| `ez2dj5th` | `ez2dj4th`에서 물려받음 | **미확정** |
| `ez2dj3rd`·`ez2dj4th`·`ez2dj6th`·`ez2d2m` | 각 근거는 프로파일 주석 참조 | 덤프 대조 **미완료** |

[1st·5th Hardlock descriptor 분석](ez2dj1st-5th-hardlock-descriptors.md)은 두 빌드가 transform loop를 넘긴 뒤 정확히 `0xc0000096`에서 멈춘다고 기록한다. 즉 **Hardlock 응답이 틀린 것이 아니라 I/O 경계가 준비되지 않은 것**일 수 있다. 확인 방법은 각 빌드의 `resumed` 덤프에 `re2dj_port_helper_scan`을 돌려 프로파일 값과 대조하는 것이다. 덤프 수집에는 실제 실행이 필요하다.

*The [1st and 5th Hardlock descriptor analysis](ez2dj1st-5th-hardlock-descriptors.md) records both builds stopping at exactly `0xc0000096` past the transform loop, so **the Hardlock response may not be wrong at all — the I/O boundary may simply not be prepared**. To find out, run `re2dj_port_helper_scan` over each build's `resumed` dump and compare with the profile. Collecting the dumps needs a real run.*

**한계.** 시그니처 탐색은 구문적이다. 같은 바이트 열이 다른 명령 안이나 코드에 박힌 데이터에 나타날 수 있으므로 결과는 후보이며, 게임이 실제로 그 helper를 부르는지는 런타임 fault 관찰이 확정한다. 시그니처 자체도 1st SE 한 빌드에서 확인한 형태이므로, 맞지 않는 빌드가 나오면 그 빌드의 실제 바이트를 근거로 항목을 추가한다.

*Limits. The search is syntactic: the same bytes can occur inside another instruction or in data embedded in code, so a hit is a candidate, and run-time fault observation still settles whether the game calls that helper. The signatures themselves are the shape confirmed in one build, so a build that does not match earns a new entry grounded in its own bytes rather than a generalization.*

### 2026-09-17 보호 빌드 덤프 대조 / Cross-check against protected-build dumps

근거: [작업 294](../work-logs/20260917-294-port-helper-dump-crosscheck.md)

**확인됨.** 각 보호 빌드를 `--image-dump`로 실행하고 `resumed` 덤프에 `re2dj_port_helper_scan`을 돌렸다. 바이트 폭 helper가 있는 다섯 빌드 모두에서 다섯 시그니처가 한 묶음으로 나왔고, `inportb`·`outportb` 주소가 **그 빌드 프로파일의 값과 정확히 일치**한다. 같은 실행의 `entry` 덤프에서는 여섯 빌드 모두 0건이다.

*Confirmed. Each protected build was run with `--image-dump` and `re2dj_port_helper_scan` was run over its `resumed` dump. All five builds with byte-width helpers yielded the five signatures as one block, and the `inportb` and `outportb` addresses **match that build's profile values exactly**. The `entry` dumps from the same runs yielded zero in all six builds.*

| 프로파일 / profile | 덤프 `inportb` | 덤프 `outportb` | 프로파일 in / out | 판정 / status |
| --- | --- | --- | --- | --- |
| `ez2dj1st` | `0x00035757` | `0x0003577b` | `0x00035757` / `0x0003577b` | **확인됨 / confirmed** |
| `ez2dj1stse` (CHD `.protect`) | `0x00038987` | `0x000389ab` | `0x00038987` / `0x000389ab` | **확인됨 / confirmed** |
| `ez2dj3rd` | `0x000a9887` | `0x000a98bb` | `0x000a9887` / `0x000a98bb` | **확인됨 / confirmed** |
| `ez2dj4th` | `0x000c3817` | `0x000c384b` | `0x000c3817` / `0x000c384b` | **확인됨 / confirmed** |
| `ez2dj5th` | `0x000ca067` | `0x000ca09b` | `0x000ca067` / `0x000ca09b` | **확인됨 / confirmed** |
| `ez2d2m` | 없음 / none | 없음 / none | `0` / `0x0000b565` | 시그니처 해당 없음 / not applicable |

이 표가 바로 위 표의 `ez2dj1stse`·`ez2dj5th`·"덤프 대조 미완료" 행을 대체한다. 두 가지를 정정한다.

*This table supersedes the `ez2dj1stse`, `ez2dj5th` and "dump cross-check pending" rows of the table above, and corrects two things.*

* **`ez2dj5th`는 4th 값을 물려받지 않았다.** 프로파일 값 `0x000ca067`/`0x000ca09b`는 4th의 `0x000c3817`/`0x000c384b`와 다르며, 2026-09-10에 5th 자신의 fault window에서 읽은 값이다. 덤프가 그것을 재확인했다.
* **1st SE CHD 빌드와 5th가 `0xc0000096`에서 멈춘다는 기록은 현재 상태가 아니다.** 그 기록은 2026-09-10 Hardlock 후보 판별 중의 관찰이며, 이후 helper RVA가 설정되면서 해소됐다. 2026-09-17 실행에서 두 빌드는 크래시 없이 각각 572·618프레임을 그렸다. 따라서 "Hardlock 응답이 아니라 I/O 경계 때문일 수 있다"는 가설은 **대상이 없어졌다.**

*`ez2dj5th` did not inherit 4th's values: its `0x000ca067` / `0x000ca09b` differ from 4th's `0x000c3817` / `0x000c384b` and were read out of 5th's own fault window on 2026-09-10, which the dump reconfirms. And the record of the 1st SE CHD build and 5th stopping at `0xc0000096` is not current: it was observed during the 2026-09-10 Hardlock candidate search and resolved once the helper RVAs were set. In the 2026-09-17 runs both builds drew 572 and 618 frames respectively without a crash, so the hypothesis that the stop was an I/O boundary rather than the Hardlock response **no longer has anything to explain.***

**확인됨.** 다섯 빌드의 helper 묶음은 **입력 쪽 배치가 같고 출력 쪽 배치는 두 갈래다.** `inportb` 기준으로 `inportw` +7, `inportl` +15는 모두 같다. `outportb`·`outportw`는 1st와 1st SE에서 +36·+48, 3rd·4th·5th에서 +52·+64다. 입력 helper 뒤의 간격만 다르므로 같은 런타임 계열의 두 판본으로 **추정**한다. 시그니처 탐색은 간격에 의존하지 않으므로 두 갈래 모두에서 동작한다.

*Confirmed. Across the five builds **the input side of the block is laid out identically and the output side comes in two variants.** From `inportb`, `inportw` is at +7 and `inportl` at +15 everywhere, while `outportb` and `outportw` sit at +36 and +48 in 1st and 1st SE but at +52 and +64 in 3rd, 4th and 5th. Only the gap after the input helpers differs, which is **inferred** to be two revisions of the same runtime family. The signature search does not depend on the gap, so it works on both.*

**확인됨 — `ez2d2m`은 시그니처 방식의 대상이 아니다.** 확인된 출력 지점 `0x0000b565`의 바이트는 `66 EF`(`out dx, ax`)이며 CRT helper가 아니라 더 큰 함수 끝에 인라인되어 있다. `mov dx, [ebx*2+0x0044d410]; mov ax, di; pop edi; pop esi; out dx, ax; pop ebx; ret 8`이다. 시그니처가 0건인 것은 스캐너의 결함이 아니라 이 빌드가 port I/O를 게임 코드에 인라인했기 때문이다.

*Confirmed — `ez2d2m` is outside what the signature method covers. The bytes at its confirmed output site `0x0000b565` are `66 EF` (`out dx, ax`), inlined at the tail of a larger function rather than a CRT helper: `mov dx, [ebx*2+0x0044d410]; mov ax, di; pop edi; pop esi; out dx, ax; pop ebx; ret 8`. Zero signature hits is not a scanner defect but a build that inlines its port I/O into game code.*

**추정 — `ez2d2m` 입력 지점 후보.** 같은 덤프에서 `66 ED`(`in ax, dx`)가 두 곳 있다. `0x0000b169`와 `0x0000b4cb`이며, 둘 다 `mov dx, [ecx]; add ecx, 2; in ax, dx; mov [edi], ax`처럼 포트 표를 차례로 읽어 버퍼에 담는 루프 모양이다. 표 주소 `0x0044d410`은 출력 지점이 쓰는 것과 같다. 다만 게스트가 입력 포트를 읽는 것이 런타임에 관측된 적은 없으므로([EZ2Dancer I/O 맵](ez2dancer-io-map.md)) 확정하지 않는다. 프로파일의 `legacy_io_in_rva = 0`은 opcode 판정으로 두 지점을 모두 처리하므로 값을 바꿀 필요도 없다.

*Inferred — candidate `ez2d2m` input sites. The same dump holds two `66 ED` (`in ax, dx`) sites, `0x0000b169` and `0x0000b4cb`, both in a loop of the shape `mov dx, [ecx]; add ecx, 2; in ax, dx; mov [edi], ax` that walks a port table into a buffer, and the table at `0x0044d410` is the same one the output site uses. The guest has never been observed reading an input port at run time ([EZ2Dancer I/O map](ez2dancer-io-map.md)), so this is not confirmed; and the profile's `legacy_io_in_rva = 0` already handles both sites by opcode, so no value needs to change.*
