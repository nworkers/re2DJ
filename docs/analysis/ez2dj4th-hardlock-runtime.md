# ez2dj4th Hardlock runtime 분석

## 확인됨

- **확인됨 — 기본 설정 적재.** 실제 CHD 제품 실행은 명시적 `--hardlock-config` 없이 Git-ignore된 `cfg/hardlock.ini`의 `ez2dj4th` section을 읽고 `hardlock_secret_config loaded=true`를 기록했습니다. 값 자체는 명령행과 JSONL/VFS 로그에 나타나지 않았습니다.
- **확인됨 — 장치 초기화 경계.** broad API software watch 없이 slot-writer hardware trace를 사용한 실제 CHD 실행은 `CreateFileA("\\.\NTICE")` 두 번, `CreateFileA("\\.\FEnteDev")` 한 번을 호출했습니다. `FEnteDev` synthetic handle의 첫 요청은 `0x9c402468`, input/output size `0/0`입니다.
- **확인됨 — active-console 진행 조건.** host WTS 상태를 그대로 전달한 실행은 `0x468` 뒤에서 더 진행하지 않았습니다. 성공한 `WTS_CURRENT_SESSION` class-4 결과만 active 상태 `0`으로 바꾼 bounded 실행은 동일한 in-place 6-byte `0x9c402450` 요청을 세 번 호출했습니다.
- **확인됨 — 4th descriptor/transform 경계.** active-console HLE에 기존 synthetic `0x450` replay와 nonzero descriptor tail 분기 실험을 결합한 bounded 실행은 initialize 1회, handshake 2회, descriptor 37회, Function `0x0e` transform 36회를 기록하고 child exit로 끝났습니다. Function 0/6 `0x44c`는 256-byte in-place descriptor이며, Function `0x0e` `0x458`은 block count 1의 264-byte in-place packet입니다. 모든 요청의 shape와 sequence가 유효했고, 모든 descriptor의 module address가 외부 profile 설정과 일치했습니다.
- **확인됨 — 값 비노출 검증 경계.** 플랫폼 중립 `HardlockProtocolTracker`는 `0x468`, `0x450`, `0x44c`, `0x458`의 순서와 크기를 판정합니다. descriptor가 도달하면 Function, block count, 설정된 module address와의 일치 여부만 기록하며 실제 module address, seed, ID 및 block byte는 기록하지 않습니다.
- **확인됨 — 공식 vendor driver 출처.** [`haspnt64` release](https://github.com/leecher1337/haspnt64/releases/tag/20221021)가 가리키는 `haspdinst.exe` 8.31을 시스템 임시 디렉터리에만 내려받아 실행하지 않고 embedded CAB을 분리했습니다. Installer의 Authenticode는 `Thales DIS CPL USA, Inc.` 서명으로 유효했고 SHA-256은 `B3CEEF35024FE9E5FC6D8B2F658CB80F56ABCE35CF435AAAB97C35F498D437CA`입니다. CAB에서 확인한 x86 `hardlock.sys` 3.92와 x64 `hardlock.sys` 3.93도 각각 `Gemalto, Inc.`의 유효한 Authenticode 서명을 가집니다. Binary와 raw disassembly는 저장소에 복사하지 않았고 installer와 driver는 실행·설치·load하지 않았습니다.
- **확인됨 — x64 driver IOCTL 계약.** x64 `hardlock.sys` 3.93의 `.text`에는 `0x9c402468`, `0x9c402450`, `0x9c40244c`, `0x9c402458` 네 상수가 각각 한 번씩 직접 dispatch 비교에 나타납니다. `0x450` 경로는 input/output을 각각 6바이트로 검사하고, `0x44c` 경로는 각각 256바이트로 검사합니다. `0x458` 경로는 input이 최소 256바이트이고 input/output 크기가 같아야 하며, descriptor offset `0x16`의 block count를 읽어 8바이트 단위 payload 길이를 처리합니다. 이 계약은 4th bounded 실행에서 관찰한 264-byte, block-count-one packet과 독립적으로 일치합니다.
- **확인됨 — Function `0x0e`의 driver 경계.** `0x458` dispatch는 공통 packet validator를 거쳐 descriptor와 `0x100` 뒤의 block payload를 내부 transport routine으로 전달합니다. 확인한 host-driver 경로에는 profile별 세 seed를 입력받는 인터페이스나 세 seed로 8바이트 응답을 계산하는 독립 transform이 없습니다. [`haspnt64`의 I/O adapter](https://github.com/leecher1337/haspnt64/blob/main/haspvdd/haspio.c)도 DOS packet을 변환한 뒤 실제 처리를 `HARDLOCK.SYS`에 위임합니다. 따라서 공식 driver는 IOCTL framing의 독립 근거이지만, dongle 내부 Function `0x0e` 알고리즘의 구현 근거는 아닙니다.

*Confirmed — An option-free real-CHD product launch loads the `ez2dj4th` section from Git-ignored `cfg/hardlock.ini` and emits only `hardlock_secret_config loaded=true`. With broad API software watches disabled, a slot-writer hardware trace calls `CreateFileA("\\.\NTICE")` twice and `CreateFileA("\\.\FEnteDev")` once; the first synthetic-handle request is zero-sized `0x9c402468`. Forwarding the host WTS state stops there, while changing only a successful current-session class-4 result to active state zero reaches six-byte `0x450`. Combining active-console HLE with the existing synthetic `0x450` replay and nonzero-tail branch experiment produces one initialize, two handshakes, 37 descriptors, and 36 Function `0x0e` transforms before child exit. Function 0/6 `0x44c` is an in-place 256-byte descriptor; Function `0x0e` `0x458` is an in-place 264-byte packet with block count one. Every shape and sequence is valid and every descriptor module address matches the external profile configuration. Logs retain only function, block count, and match Booleans—never the configured address, seeds, IDs, or block bytes. Static-only inspection of the Thales 8.31 installer and its validly signed Gemalto x86 3.92/x64 3.93 `hardlock.sys` drivers independently confirms the exact x64 dispatch constants. The driver requires 6/6 bytes for `0x450`, 256/256 bytes for `0x44c`, and equal input/output sizes of at least 256 bytes for `0x458`; the latter reads descriptor block count at `0x16` and handles payload in eight-byte units. It then sends the descriptor and blocks through an internal transport boundary. No host interface or independent host transform consuming three profile seeds was found. The official driver therefore confirms framing but not the dongle-internal Function `0x0e` algorithm. No installer or driver was executed, installed, or loaded, and no downloaded binary or raw disassembly entered the repository.*

- **확인됨 — 장치 경계 스텁.** 플랫폼 중립 `HardlockStubDevice`는 확인된 vendor framing(`0/0`, `6/6`, `256/256`, `256 + block_count × 8`)에 맞는 요청만 완료 처리하고, 크기·block count·buffer 겹침이 어긋나면 거절합니다. `0x468`은 성공만 반환하고, `0x450`은 설정된 replay 또는 요청 buffer 보존, `0x44c`/`0x458`은 요청 보존과 status word `0` 정리로 응답합니다. Function `0x0e` payload는 변환하지 않고 그대로 통과시킵니다. 이 계약은 unit test로 고정했습니다.

*Confirmed — device-boundary stub. The platform-neutral `HardlockStubDevice` completes only requests matching the confirmed vendor framing (`0/0`, `6/6`, `256/256`, and `256 + block_count × 8`) and rejects mismatched sizes, block counts, or partially overlapping buffers. It answers `0x468` with success alone, `0x450` with a configured replay or the preserved request buffer, and `0x44c`/`0x458` with the preserved request plus a cleared status word, passing the Function `0x0e` payload through untransformed. Unit tests pin this contract.*

- **확인됨 — 장치 경계 스텁 실제 실행.** 실제 CHD bounded 실행 두 번으로 스텁 경로를 확인했습니다. 분기 실험값 없이 실행하면 initialize 1회와 handshake 3회가 모두 `outcome=completed`(`replayed=0`, 요청 buffer 보존)로 처리된 뒤 `0x44c`에 도달하지 못하고 child가 `0x00000008`로 종료합니다. 기존 `0100fafa0010` replay와 `tail=0x0001`을 함께 주면 initialize 1회, handshake 2회, descriptor 37회, transform 36회가 모두 `outcome=completed`이고 `rejected-shape`는 0회입니다. 이 횟수는 Task 127이 두 개의 개별 분기로 관찰한 값과 정확히 일치합니다.
- **확인됨 — transform loop 이후 fault.** 위 두 번째 실행은 `0xc0000005` write fault로 종료합니다. `eip=0x004c440b`(`.text`), `esp=0x75295d4d`, `ebp=0xc415ff50`이고 fault 주소 `0x75295d49`는 `esp - 4`입니다. `esp`는 `kernel32` 이미지 주소 범위 안에 있습니다.

*Confirmed — device-boundary stub in a real run. Two bounded real-CHD runs exercised the stub path. Without branch-experiment values, one initialize and three handshakes all completed with `replayed=0` (request buffer preserved), then the child exited `0x00000008` without reaching `0x44c`. Supplying the existing `0100fafa0010` replay and `tail=0x0001` produced one initialize, two handshakes, 37 descriptors, and 36 transforms, all `outcome=completed` with zero `rejected-shape` — counts matching exactly what Task 127 observed through two separate branches.*

*Confirmed — fault after the transform loop. That second run ends in a `0xc0000005` write fault at `eip=0x004c440b` in `.text`, with `esp=0x75295d4d`, `ebp=0xc415ff50`, and faulting address `0x75295d49`, which is `esp - 4` and lies inside the `kernel32` image range.*

- **확인됨 — `.text`는 파일에서 암호화되어 있다.** 4th `EZ2DJ.EXE`의 `.text`는 `vaddr 0x1000 / raw 0x1000`이라 RVA와 file offset이 같습니다. 901,120바이트의 Shannon 엔트로피는 `7.9967` bits/byte이고 `55 8b ec` prologue와 `cc` padding이 각각 0회입니다.
- **확인됨 — `.text`는 실행 중 다시 쓰인다.** VA `0x004c4400`(file offset `0x000c4400`)의 32바이트는 파일과 실행 중 memory가 서로 다릅니다. fault 지점 `0x004c440b`의 실행 중 바이트는 `0x55`(`push ebp`)이며 기록된 `esp - 4` write fault와 일치합니다.
- **확인됨 — 게스트는 Function `0x0e` 출력을 소비한다.** 다른 옵션이 모두 같고 IOCTL 처리 결과도 같은(initialize 1, handshake 2, descriptor 37, transform 36, 전부 `completed`) 두 실행에서 transform 출력 8바이트만 XOR `0xff`로 뒤집자 fault가 `eip=0x004c440b`, write `0x75295d49`, `esp=0x75295d4d`에서 `eip=0x0024d000`, write `0x00000000`, `esp=0x001beb34`로 바뀌었습니다. 출력 값이 이후 실행 경로를 결정합니다.

*Confirmed — `.text` is encrypted on disk. For 4th `EZ2DJ.EXE`, `.text` maps `vaddr 0x1000` to `raw 0x1000`, so RVA equals file offset; its 901,120 bytes have Shannon entropy `7.9967` bits/byte with zero `55 8b ec` prologues and zero `cc` padding.*

*Confirmed — `.text` is rewritten at runtime. The 32 bytes at VA `0x004c4400` (file offset `0x000c4400`) differ between the file and process memory, and the runtime byte at the fault point `0x004c440b` is `0x55` (`push ebp`), matching the recorded `esp - 4` write fault.*

*Confirmed — the guest consumes the Function `0x0e` output. Between two runs with every other option identical and identical IOCTL results (one initialize, two handshakes, 37 descriptors, 36 transforms, all completed), inverting only the eight transform output bytes with XOR `0xff` moved the fault from `eip=0x004c440b`, write `0x75295d49`, `esp=0x75295d4d` to `eip=0x0024d000`, write `0x00000000`, `esp=0x001beb34`. The output value determines the downstream execution path.*

- **확인됨 — 제품 실행 경로 도달 지점.** `run_detached`와 active-console 정책을 profile 기본값으로 둔 뒤, 실제 CHD 제품 실행 `re2dj.exe ez2dj4th --run`은 detach 후 initialize 1회와 handshake 3회를 거쳐 `0x00000008`로 종료합니다. 이는 분기 실험값 없는 bounded launcher 실행과 같은 지점입니다. 제품 실행 경로에는 합성 응답 옵션이 없으므로 이 지점은 장치 경계 스텁 없이 도달한 결과입니다. 스텁을 켠 launcher 실행도 같은 지점에서 멈추므로, 스텁은 도달 지점을 바꾸지 않고 요청 계약만 검증합니다.

*Confirmed — where the product path reaches. With `run_detached` and the active-console policy as profile defaults, a real-CHD `re2dj.exe ez2dj4th --run` detaches, passes one initialize and three handshakes, and exits `0x00000008` — the same point a bounded launcher run without branch-experiment values reaches. The product path carries no synthetic-response option, so this point is reached without the device-boundary stub; a launcher run with the stub enabled stops at the same point, so the stub does not change where execution reaches and only validates the request contract.*

- **확인됨 — `0x450` 검사 범위.** 원본은 6바이트 `0x450` 응답 중 byte 2와 byte 3만 검증하며 두 바이트는 정확히 일치해야 합니다. byte 0·1·4·5를 바꾼 실행은 descriptor 37회와 transform 36회를 그대로 기록하고, byte 2 또는 byte 3을 바꾼 실행은 descriptor에 도달하지 못한 채 handshake 3회 뒤 `0x00000008`로 종료합니다. 인접값 한 바이트 변경도 실패하므로 범위나 구조 검사가 아닙니다. 근거는 [Task 133 작업 로그](../work-logs/20260902-133-ez2dj4th-protection-shape.md)입니다.
- **확인됨 — 원본 이미지는 전 구간 암호문이다.** (보호 대상 범위는 Task 135에서 `.idata`와 `.protect`를 제외한 전체로 확정되었습니다.) `.text` 901,120바이트는 64 KiB 단위 14개 구간 전부 엔트로피 `7.997`이고 zero byte 비율이 균등난수 기대값 부근이며, `55 8b ec` prologue와 `cc` padding이 각각 0회입니다. `.rdata` `7.896`, `.data` `7.998`, `.protect` `7.969`로 다른 섹션도 고엔트로피입니다. `.text`에 평문 구간은 없습니다.
- **확인됨 — 보호를 건너뛰어 진입점으로 갈 수 없다.** `.text`가 전 구간 암호문이고, 보호 코드가 실행 중 `.text`를 다시 쓰며, Function `0x0e` 출력이 그 결과를 결정합니다. 따라서 Hardlock은 게임 앞의 검문소가 아니라 게임을 복호화하는 열쇠 공급원이며, 건너뛰면 도착지는 진입점이 아니라 암호문입니다.

*Confirmed — the scope of the `0x450` check. The original validates only bytes 2 and 3 of the six-byte `0x450` response and requires an exact match. Changing byte 0, 1, 4, or 5 still records 37 descriptors and 36 transforms, while changing byte 2 or 3 stops before any descriptor, ending after three handshakes with `0x00000008`; even a one-byte change to an adjacent value fails, so this is not a range or structural check. The evidence is the [Task 133 work log](../work-logs/20260902-133-ez2dj4th-protection-shape.md).*

*Confirmed — the original image is ciphertext throughout. `.text`'s 901,120 bytes read entropy `7.997` across all fourteen 64 KiB chunks with zero-byte shares near the uniform-random expectation and zero `55 8b ec` prologues and `cc` padding; `.rdata` at `7.896`, `.data` at `7.998`, and `.protect` at `7.969` are likewise high entropy. `.text` contains no plaintext region.*

*Confirmed — the protection cannot be skipped to reach the entry point. `.text` is ciphertext throughout, the protection rewrites it at runtime, and the Function `0x0e` output determines that result, so Hardlock is the key source that decrypts the game rather than a checkpoint in front of it; skipping it arrives at ciphertext rather than at an entry point.*

- **확인됨 — 4th descriptor 식별값.** `--hardlock-descriptor-ids` 진단으로 두 번 독립 실행한 결과가 일치했습니다. API version `0347`, module id `0x0000`, module address `0x4c53`, remote `1`, port `0x0378`, `ID_Ref=a755931881fd81ea`, `ID_Verify=ceed1a5e4f27078f`입니다. 3rd는 module address `0x4c51`, `ID_Ref=478c8b793f201f8a`, `ID_Verify=cc22ae2da344b2a2`이므로 4th는 별개 module입니다. 이 값들은 seed 자체나 유효한 Function `0x0e` 응답이 아니라 seed 복구 제약식의 입력입니다. 근거는 [Task 134 작업 로그](../work-logs/20260902-134-hardlock-stub-rescope.md)입니다.

*Confirmed — 4th descriptor identity values. Two independent runs with the `--hardlock-descriptor-ids` diagnostic agreed on API version `0347`, module id `0x0000`, module address `0x4c53`, remote `1`, port `0x0378`, `ID_Ref=a755931881fd81ea`, and `ID_Verify=ceed1a5e4f27078f`. 3rd carries module address `0x4c51` with `ID_Ref=478c8b793f201f8a` and `ID_Verify=cc22ae2da344b2a2`, so 4th is a separate module. These values are inputs to seed-recovery constraints rather than seeds or valid Function `0x0e` responses; the evidence is the [Task 134 work log](../work-logs/20260902-134-hardlock-stub-rescope.md).*

- **확인됨 — Function `0x0e` challenge의 출처.** 4th의 36개 challenge block은 원본 이미지에서 32 KiB 청크 시작 8바이트와 정확히 일치합니다. 청크는 section별로 raw 시작에서 `0x8000` 간격으로 걷습니다. `.text`(`0x001000`) 28개, `.rdata`(`0x0dd000`) 2개, `.data`(`0x0ea000`) 4개, `.reloc`(`0x108000`) 2개로 합계 36개입니다. `.idata`(`0x106000`)와 `.protect`(`0x115000`)에는 challenge가 없습니다. 두 독립 실행이 동일한 목록을 기록했고 고유값은 32개, 중복 4개입니다. 근거는 [Task 135 작업 로그](../work-logs/20260902-135-hardlock-transform-response-map.md)입니다.
- **확인됨 — 응답 주입 경계.** 외부에서 계산한 challenge→response 매핑을 원본 실행에 전달하는 경계가 동작합니다. 수집한 challenge에 XOR `0xff` 출력을 부여한 합성 매핑 실행은 36개 요청 전부를 `mapped=1:unmapped=0`으로 처리했고, fault 시점 레지스터가 기존 `--hardlock-transform-xor ff` probe 실행과 일치했습니다.
- **확인됨 — fault 주소는 판별 기준이 아니다.** 위 두 실행의 `eip`는 `0x0024d000`과 `0x00313000`으로 달랐고 둘 다 이미지 밖 주소입니다. fault 주소는 실행마다 달라지는 할당 주소이므로 후보 판별에 그대로 쓸 수 없습니다.

*Confirmed — where the Function `0x0e` challenges come from. 4th's 36 challenge blocks match the first eight bytes of each 32 KiB chunk of the original image exactly, walked per section from each section's raw start at `0x8000` intervals: 28 in `.text` (`0x001000`), 2 in `.rdata` (`0x0dd000`), 4 in `.data` (`0x0ea000`), and 2 in `.reloc` (`0x108000`), totalling 36, with no challenge for `.idata` (`0x106000`) or `.protect` (`0x115000`). Two independent runs recorded the same list, with 32 unique values and four repeats; the evidence is the [Task 135 work log](../work-logs/20260902-135-hardlock-transform-response-map.md).*

*Confirmed — the response injection boundary. The boundary carrying an externally computed challenge-to-response map into the original execution works: a synthetic map assigning each collected challenge its XOR `0xff` output handled all 36 requests as `mapped=1:unmapped=0`, and the fault-time registers matched the earlier `--hardlock-transform-xor ff` probe run.*

*Confirmed — the fault address is not a discriminator. Those two runs faulted at `eip=0x0024d000` and `0x00313000`, both outside any image. The fault address is a per-run allocation address and cannot be used directly to judge candidates.*

- **추정 — `.idata` 제외 이유.** Windows loader가 import table을 평문으로 읽어야 하므로 제외된 것으로 보입니다. 이 귀속은 별도로 확인하지 않았습니다.

*Inferred — why `.idata` is excluded. It is likely excluded because the Windows loader must read the import table as plaintext; this attribution was not separately verified.*

- **확인됨 — challenge 유도 규칙.** 4th의 36개 Function `0x0e` challenge는 원본 실행 파일만으로 재현됩니다. 각 PE section에 대해 raw 시작에서 `0x8000` 간격으로 걷고 각 지점의 8바이트를 취하며, 다음 지점이 section raw 범위를 벗어나면 그 section을 끝내고, `.idata`와 `.protect`를 제외합니다. 이 규칙으로 유도한 36개는 관찰 목록과 **값과 순서까지 정확히 일치**했습니다. 근거는 [Task 136 작업 로그](../work-logs/20260902-136-resoftlock-interface-contract.md)입니다.

*Confirmed — the challenge derivation rule. 4th's 36 Function `0x0e` challenges can be reproduced from the original executable alone: for each PE section, walk from its raw start at `0x8000` intervals taking eight bytes at each point, stop when the next point leaves the section's raw range, and skip `.idata` and `.protect`. The 36 challenges derived this way matched the observed list **exactly in both value and order**; the evidence is the [Task 136 work log](../work-logs/20260902-136-resoftlock-interface-contract.md).*

- **확인됨 — 보호를 통과하는 응답 집합이 존재한다.** [Task 139](../work-logs/20260902-139-hardlock-candidate-judgement.md)에서 외부 생성기의 후보 93개를 모두 주입한 결과, 정확히 하나(`candidate-68`)만 다른 실행 형태를 보였습니다. 나머지 92개는 transform loop 직후 fault로 끝나고(`0xc0000005` 59, `0xc0000096` 19, `0xc000001d` 10, 기타 4), 그 후보만 종료 코드 `0x00000000`으로 끝나며 vfs trace가 451줄에서 468줄로 늘고 `0x450` handshake가 4회에서 8회가 되며 `\\.\FEnteDev`를 다시 열고 descriptor `function=1`이 처음 나타나고 `EZ2DJ.ini`를 엽니다. 세 번 재실행에서 동일하게 재현됐습니다. 주입은 93회 모두 `mapped=36:unmapped=0`으로 완전했습니다. seed 값과 응답 바이트는 이 저장소에 기록하지 않습니다.
- **추정 — 그 후보가 물리 dongle seed다.** 확인된 것은 원본이 그 응답을 받아들이고 보호를 통과한다는 사실입니다. 그 값이 물리 dongle 내부 seed와 같은지는 직접 관찰하지 않았습니다.

*Confirmed — a response set that passes the protection exists. Injecting all 93 external-generator candidates in Task 139 left exactly one, `candidate-68`, with a different run shape: the other 92 end in a fault right after the transform loop, while that one exits `0x00000000`, grows the vfs trace from 451 to 468 lines, doubles the `0x450` handshakes from four to eight, reopens `\\.\FEnteDev`, produces the first observed `function=1` descriptors, and opens `EZ2DJ.ini`. It reproduced identically across three runs, and all 93 runs injected completely at `mapped=36:unmapped=0`. Seed values and response bytes are not recorded in this repository.*

*Inferred — that candidate holds the physical dongle's seeds. What is confirmed is that the original accepts its responses and passes the protection; whether the values equal the dongle's internal seeds was not directly observed.*

- **확인됨 — 섹션 점수가 재현된다.** [Task 137](../work-logs/20260902-137-decrypted-region-judge.md)의 `re2dj_code_score`가 원본 CHD의 `EZ2DJ.EXE`를 읽어 `.text` `7.9967`(prologue 0, `cc` run 0), `.rdata` `7.8962`, `.data` `7.9975`, `.protect` `7.9690`을 보고했습니다. 이전에 임시 계산으로 기록한 값과 일치하므로 기록과 측정기가 서로를 검증합니다. `.idata`는 `3.4734`, `.reloc`은 `7.7558`입니다.
- **확인됨 — `.protect` 머리 1 KiB는 평문 코드다.** 파일 offset `0x115000`부터 1,024바이트 구간은 엔트로피 `2.8456`, `55 8b ec` 3회, zero byte `65.7%`입니다. `.protect`의 3회는 섹션 전체에서 관찰된 prologue 전부이며, 균등 난수 237,568바이트의 기대값은 약 `0.014`회입니다. 같은 섹션의 `+0x1000` 이후 구간은 `7.97` 부근입니다. 따라서 "이미지 전 구간이 암호문"이라는 서술의 예외는 이 1 KiB이며, `.text`에 평문 구간이 없다는 결론은 그대로입니다.

*Confirmed — the section scores reproduce. Task 137's `re2dj_code_score`, reading `EZ2DJ.EXE` from the original CHD, reported `.text` at `7.9967` with zero prologues and zero `cc` runs, `.rdata` at `7.8962`, `.data` at `7.9975`, and `.protect` at `7.9690`, matching the figures recorded earlier from ad hoc calculation, so the record and the measurement tool validate each other; `.idata` reads `3.4734` and `.reloc` `7.7558`.*

*Confirmed — the first 1 KiB of `.protect` is plaintext code. The 1,024 bytes at file offset `0x115000` read entropy `2.8456` with three `55 8b ec` occurrences and `65.7%` zero bytes. Those three are every prologue observed in the whole section, against an expectation of about `0.014` for 237,568 uniform random bytes, and the section reads near `7.97` from `+0x1000` onward. This 1 KiB is therefore the exception to "the image is ciphertext throughout"; the conclusion that `.text` holds no plaintext region is unchanged.*

- **추정 — `.protect` 머리 stub의 역할.** 평문으로 남은 이 1 KiB가 보호의 최초 실행 코드로 보입니다. 무엇인가는 복호화 이전에 실행되어야 하고, 진입점 RVA `0x006e0240`이 `.text` 밖이라는 기존 관찰과 일관됩니다. 다만 이 구간이 실제로 진입점을 포함하는지는 확인하지 않았습니다.

*Inferred — the role of the `.protect` head stub. The 1 KiB left in plaintext reads as the protection's first executing code: something must run before decryption, and this is consistent with the existing observation that entry RVA `0x006e0240` lies outside `.text`. Whether this range actually contains the entry point was not verified.*

## 미확정

- **추정 — transform 출력의 역할.** `.text`가 파일에서 암호화되어 있고 실행 중 다시 쓰인다는 사실과, transform 출력이 실행 경로를 바꾼다는 사실을 함께 보면 그 출력이 보호 복호화의 key material로 쓰인다고 보는 것이 자연스럽습니다. 다만 key 유도 경로 자체를 직접 관찰하지는 않았습니다.
- **확인됨 — 분기 강제로는 대체 불가.** 위 인과 결과에 따라 이 blocker는 뒤집을 보호 분기 하나가 아닙니다. 따라서 게스트 분기 강제 방식의 우회는 올바른 Function `0x0e` 결과를 대체할 수 없습니다.

*Inferred — the role of the transform output. Taken together, `.text` being encrypted on disk and rewritten at runtime, plus the transform output changing the execution path, make it natural to read that output as key material for the protection's decryption, though the key-derivation path itself was not directly observed.*

*Confirmed — branch forcing is not a substitute. Given the causality result, this blocker is not a single protection branch, so a guest branch-forcing bypass cannot substitute for a correct Function `0x0e` result.*

- **미확정 — 스텁 실행 관찰의 지위.** 스텁이 켜진 실행에서 관찰한 동작은 원본 동작이 아닙니다. 스텁은 장치 경계의 API 성공만 보장하며, 응답 내용은 실제 dongle 값이 아닙니다.
- **미확정 — `0x450` byte 2·3의 출처.** 통과하는 값은 알고 있지만, 그 16비트가 실제 dongle 응답에서 어떻게 나오는지는 확인하지 않았습니다. 요청 buffer를 보존하는 success 정책은 이 검사를 통과하지 못합니다.
- **미확정 — synthetic 분기값의 진위.** `0x44c/0x458` 도달에 사용한 replay와 tail은 보호 코드의 다음 경계를 확인하기 위한 기존 synthetic 실험값입니다. 실제 driver 또는 dongle response라는 근거는 없으며 profile 비밀값에서 유도된 값도 아닙니다.
- **미확정 — Function `0x0e` 변환.** 세 seed를 사용하는 bit-level transform의 허용 가능한 독립 근거와 알려진 입출력 vector를 확보하지 못했습니다. 공개 copyleft 구현은 프로젝트 정책에 따라 사용하지 않았으며, 알고리즘을 추측해 제품 코드에 넣지 않았습니다.
- **미확정 — x86 3.92의 동일 dispatch 위치.** x86 driver에는 네 IOCTL 값이 직접 little-endian 상수로 나타나지 않아 이번 범위에서 x64와 같은 dispatch 위치를 독립적으로 특정하지 않았습니다. 4th의 실제 32-bit caller 계약은 원본 실행 계측으로 이미 확인됐지만, x86 vendor driver 내부 귀속은 별도 control-flow 복원이 필요합니다.

*Unresolved — Buffer-preserving success does not pass the `0x450` boundary, so a real driver-written six-byte response or independently verified behavior is required. The replay and tail used to reach `0x44c/0x458` are existing synthetic branch experiments, not verified driver/dongle responses and not values derived from the profile secrets. No policy-compatible independent basis or known input/output vector for the three-seed Function `0x0e` bit-level transform has been obtained. Public copyleft implementations were not used, and no guessed algorithm was added to product code. The x86 3.92 driver does not contain the four IOCTL values as direct little-endian constants, so this task does not claim the corresponding internal x86 dispatch locations; recovering those locations would require a separate control-flow analysis.*

## 2EZConfig-V2 참고 결과

- **확인됨 — 라이선스와 범위.** 사용자가 제시한 [2EZConfig-V2 Hardlock directory](https://github.com/ben-rnd/2EZConfig-V2/tree/master/src/libs/hardlock)를 별도 임시 clone으로 확인했습니다. 저장소 root [LICENSE](https://github.com/ben-rnd/2EZConfig-V2/blob/master/LICENSE)는 GPL-3.0-or-later이며, Hardlock 파일에는 re2DJ가 사용할 수 있는 별도 permissive license가 확인되지 않았습니다. 따라서 source는 복사·번역·link하지 않았습니다.
- **확인됨 — high-level 계약 대조.** 해당 소스는 `API_CRYPT`를 Function 14로 구분하고, Function `0x0e` 처리에서 descriptor 뒤 offset `0x100`부터 `Bcnt`개의 8바이트 block을 in-place 처리하는 형태를 보여 줍니다. 같은 primitive를 초기 ID pair 확인에도 사용하는 구조가 보이지만, 이는 GPL source에서 관찰한 high-level 계약일 뿐 re2DJ의 독립 구현 근거 또는 vendor/dongle response vector가 아닙니다.
- **추정 — backend 설계 적합성.** 위 계약은 현재 원본에서 확인된 `0x458`, descriptor `0x16` block count, `0x100` payload 경계와 일치하므로 별도 `HardlockDeviceBackend`의 request shape 설계에는 참고가 됩니다. 그러나 계약만으로 seed algorithm이나 유효 response를 결정할 수는 없습니다.
- **미확정 — 인증 가능한 응답.** 별도 자동화 test vector나 물리 동글 output이 확인되지 않았습니다. GPL 구현의 bit-level primitive를 재작성하거나 기존 synthetic replay/tail을 제품 기본값으로 승격하는 것은 현재 작업 범위와 프로젝트 라이선스·증거 정책에 맞지 않습니다.

*Confirmed — license and scope. The user-provided [2EZConfig-V2 Hardlock directory](https://github.com/ben-rnd/2EZConfig-V2/tree/master/src/libs/hardlock) was inspected in a separate temporary clone. The repository root [LICENSE](https://github.com/ben-rnd/2EZConfig-V2/blob/master/LICENSE) is GPL-3.0-or-later, and no separate permissive license usable by re2DJ was found for the Hardlock files. Its source was therefore not copied, translated, or linked.*

*Confirmed — high-level contract comparison. The source distinguishes `API_CRYPT` as Function 14 and shows Function `0x0e` processing `Bcnt` eight-byte blocks in place starting at offset `0x100` after the descriptor. It also appears to use the same primitive for the initial ID-pair check, but this is only a high-level contract observed in GPL source, not an independent implementation basis or vendor/dongle response vector for re2DJ.*

*Inferred — backend design fit. The contract aligns with the original's observed `0x458`, descriptor block count at `0x16`, and payload boundary at `0x100`, so it is useful for shaping a separate `HardlockDeviceBackend` request interface. The contract alone cannot determine the seed algorithm or a valid response.*

*Unresolved — authenticating response. No independent automated test vector or physical-dongle output was found. Rewriting the GPL bit-level primitive or promoting the existing synthetic replay/tail to a product default would violate the current project license and evidence policy.*

## 2026-09-03 VFS 절대 경로 수정 후 실행 결과

- **확인됨 — Hardlock 이후 VFS 경계 진행.** `cfg/hardlock.ini`의 4th 응답 재료와 36개 transform map이 계속 적용되었고, 최신 실행은 transform 36회를 모두 `mapped=1:unmapped=0`으로 처리했습니다.
- **확인됨 — host absolute path 재해석.** staging 아래의 `C:\...\EZ2DJ\EZ2DJ.ini` 요청이 더 이상 `C:\...\EZ2DJ\C:\...`로 이중 결합되지 않았습니다. VFS trace에서 요청과 매핑 결과가 동일한 staging 파일로 기록되며 `success=1:error=0`입니다.
- **확인됨 — CHD와 overlay 경로 보존.** 설정된 HDD root 및 HLE Windows root 아래의 host absolute path를 기존 상대 suffix 경로로 되돌리고, read/overlay 우선순위와 copy-on-write 정책을 유지하도록 runtime probe 회귀 검증을 추가했습니다.
- **미확정 — 다음 실행 경계.** `EZ2DJ.ini`를 성공적으로 연 뒤 child가 `0xc0000096`으로 종료했습니다. 이 실행 기록에는 fault instruction address가 없으므로 privileged instruction 또는 raw port I/O라고 단정하지 않으며, 다음 작업에서 예외 지점 계측으로 확인해야 합니다.

*Confirmed — Hardlock-to-VFS progression. The 4th response material from `cfg/hardlock.ini` and the 36-entry transform map remained active, and the latest run completed all 36 transforms as `mapped=1:unmapped=0`.*

*Confirmed — host-absolute path remapping. The staging path `C:\...\EZ2DJ\EZ2DJ.ini` is no longer double-joined as `C:\...\EZ2DJ\C:\...`. The VFS trace records the request and mapped result as the same staging file with `success=1:error=0`.*

*Confirmed — CHD and overlay policy preserved. Host-absolute paths under the configured HDD and HLE Windows roots are converted back to the existing relative suffix paths, while read/overlay precedence and copy-on-write behavior remain covered by runtime-probe regression checks.*

*Unresolved — next execution boundary. After successfully opening `EZ2DJ.ini`, the child exits with `0xc0000096`. The run record contains no faulting instruction address, so this is not yet attributed to a privileged instruction or raw port I/O; the next task must capture the exception point.*

## 2026-09-03 privileged instruction 계측 결과

- **확인됨 — 4th raw I/O fault 위치.** 실제 CHD staging root에 attached debugger를 유지한 `--slot-writer-trace` 실행을 추가 계측한 결과, `0xc0000096`의 first chance와 second chance가 모두 원본 image의 `0x004c3817`에서 발생했습니다.
- **확인됨 — 명령어와 port.** fault 지점의 bytes는 `ec c3...`이며 `0xec`는 `IN AL,DX`, `0xc3`는 `RET`입니다. 계측된 EDX는 `0x00ac0103`이고 low word port는 `0x0103`입니다.
- **확인됨 — 기존 경계 이후 발생.** 같은 실행에서 `response450=true`, `tail44c=true`, `map=true`, transform 36회 `mapped=1:unmapped=0`, `EZ2DJ.ini` `success=1:error=0`가 먼저 기록되었습니다. 이후 예외가 처리되지 않고 child가 `0xc0000096`으로 종료했습니다.
- **미확정 — 반환값과 HLE 정책.** 계측된 EAX low byte `0x00`은 실제 `IN` 수행 전 context 값이며 장치가 반환해야 할 값이 아닙니다. port `0x0103`의 의미와 반환 byte는 아직 확인되지 않았으므로 4th raw-I/O HLE를 기본 활성화하지 않습니다.

*Confirmed — 4th raw-I/O fault location. An attached-debugger `--slot-writer-trace`
run against the real CHD staging root, with the added instrumentation, observed both
the first- and second-chance `0xc0000096` at original image address `0x004c3817`.*

*Confirmed — instruction and port. The fault bytes begin `ec c3...`; `0xec` is
`IN AL,DX` and `0xc3` is `RET`. The captured EDX is `0x00ac0103`, making the low-word
port `0x0103`.*

*Confirmed — after the existing boundary. The same run first records
`response450=true`, `tail44c=true`, `map=true`, all 36 transforms as
`mapped=1:unmapped=0`, and `EZ2DJ.ini` with `success=1:error=0`. The unhandled
exception then causes child exit `0xc0000096`.*

*Unresolved — return value and HLE policy. The captured EAX low byte `0x00` is the
pre-`IN` context value, not the byte the device would return. The meaning and return
value for port `0x0103` remain unconfirmed, so 4th raw-I/O HLE is not enabled by default.*

## 2026-09-03 shared raw-I/O diagnostic

- **확인됨 — 4th read를 공용 bus가 처리함.** `20260903-004511-065.jsonl`에서 기존 Hardlock material과 36개 transform 및 `EZ2DJ.ini` read가 먼저 성공한 뒤, `0x004c3817`의 `0xc0000096` privileged event가 기록되었습니다. 프로파일별 RVA 연결 후 같은 주소에 대한 `io_port_read`가 `0x0103 -> 0x80`, `0x0104 -> 0x80`, `0x0105 -> 0x00`으로 기록되었고, 이후 child는 `0x00434137`의 `0xc0000005` AV까지 진행했습니다.
- **확인됨 — 4th의 현재 주소 범위.** 4th profile은 main-image byte-read RVA `0x000c3817`만 보유합니다. byte-write RVA는 0으로 남겨 두었으며 확인되지 않은 `OUT` trap은 추가하지 않았습니다.
- **확인됨 — injected runtime 경로.** 명시적 `--hle-io-ports --run-detached` 실행 `20260903-004600-318.jsonl`은 runtime policy를 `in_rva=0x000c3817`, `out_rva=0x00000000`으로 준비했고 child exit code는 `0xc0000005`였습니다. 이는 runtime handler가 이전 `0xc0000096` 경계를 넘긴다는 실행 증거입니다.
- **추정/미확정 — 응답 의미.** `0x0103`과 `0x0104`의 `0x80`은 `Ez2DjIoBoard`의 초기 turntable center 값이며, `0x0105`의 `0x00`은 초기 coin counter입니다. 이 값들이 4th 물리 보드의 정답 응답인 것은 확인되지 않았습니다. 따라서 4th raw I/O는 제품 기본값으로 승격하지 않았습니다.

* **Confirmed — the shared bus handles the 4th read.** In `20260903-004511-065.jsonl`, the existing Hardlock material, all 36 transforms, and the `EZ2DJ.ini` read succeed before the `0xc0000096` privileged event at `0x004c3817`. After the profile-specific RVA connection, `io_port_read` records `0x0103 -> 0x80`, `0x0104 -> 0x80`, and `0x0105 -> 0x00` at the same helper, and the child advances to an `0xc0000005` access violation at `0x00434137`.
* **Confirmed — the current 4th address range.** The 4th profile owns only main-image byte-read RVA `0x000c3817`. Its byte-write RVA remains zero, and no unconfirmed `OUT` trap was added.
* **Confirmed — injected runtime path.** The explicit `--hle-io-ports --run-detached` run `20260903-004600-318.jsonl` prepares the runtime policy as `in_rva=0x000c3817`, `out_rva=0x00000000`, and the child exits with `0xc0000005`. This is execution evidence that the runtime handler passes the previous `0xc0000096` boundary.
* **Inferred/unresolved — response meaning.** `0x80` at ports `0x0103` and `0x0104` is the `Ez2DjIoBoard` initial turntable-center value, while `0x00` at `0x0105` is the initial coin counter. These values are not confirmed as the physical 4th board response, so 4th raw I/O is not promoted to a product default.

관련 설계와 실행 절차는 [Task 144 설계](../design/20260903-144-ez2dj4th-profile-io-reuse.md), [Task 144 작업 지시서](../work-orders/20260903-144-ez2dj4th-profile-io-reuse.md), [Task 144 작업 로그](../work-logs/20260903-144-ez2dj4th-profile-io-reuse.md)에 둡니다.

The related design, work order, and execution record are [Task 144 design](../design/20260903-144-ez2dj4th-profile-io-reuse.md), [Task 144 work order](../work-orders/20260903-144-ez2dj4th-profile-io-reuse.md), and [Task 144 work log](../work-logs/20260903-144-ez2dj4th-profile-io-reuse.md).

## 2026-09-03 AV null receiver attribution

- **확인됨 — faulting instruction과 receiver.** `20260903-012922-258.jsonl`에서 thread `18688`이 `0x00434137`의 read AV(`0x00000014`)를 재현했습니다. runtime bytes는 `8b 48 14`(`mov ecx, [eax+0x14]`)이며 AV register는 `EAX=0`, `ECX=0`입니다.
- **확인됨 — runtime target chain.** stack direct-call candidate `0x00402275`의 runtime bytes는 `e9 9c 1e 03 00`으로 `0x00434116`으로 jump합니다. `0x00434116`은 `ECX`를 local에 저장한 뒤 `[EAX+0x14]`를 읽는 member-like routine이므로, 직접적인 fault 원인은 null object receiver입니다.
- **확인됨 — immediate caller가 null을 전달함.** runtime callsite `0x00417da4`는 `mov [EBP-8], ECX` 후 `mov ECX, [EBP-8]`을 수행하고 `call 0x00402275`를 실행합니다. `av_caller_frame`은 이 caller local을 `0x00000000`으로 기록했습니다. callee return address는 `0x00417dc7`, caller return address는 `0x0041a6a4`입니다.
- **확인됨 — 한 단계 위의 field가 zero임.** `0x0041a684` 부근 runtime window는 `mov ECX, [EBP-0x118]`, `mov ECX, [ECX+0x11c]`, `call 0x00402298` 순서입니다. `av_outer_frame`은 `[outer EBP-0x118] = 0x00acd708` 및 `[(0x00acd708)+0x11c] = 0x00000000`을 기록했습니다. 이 값은 `0x00417da4`에 전달되는 null의 가장 가까운 확인 지점입니다.
- **확인됨 — Hardlock 성공 경계와 직접 AV를 분리함.** 별도 run `20260903-010325-894.jsonl`에서 `0x450`은 `0100fafa0010` 응답과 `EAX=1`을 기록했고, `20260903-010211-910.jsonl`에서 마지막 `0x44c`는 256-byte output과 `EAX=1`을 기록했습니다. 따라서 현재 증거는 `DeviceIoControl`의 null/실패 반환을 직접적인 AV 원인으로 지지하지 않습니다.
- **확인됨 — raw-I/O 반환과 저장 위치.** `20260903-010808-683.jsonl`에서 `0x0103 -> 0x80`, `0x0104 -> 0x80`, `0x0105 -> 0x00`이 기록됐고, 각 반환-site는 값을 원본 객체의 `+0xb3c`, `+0xb40`, `+0xb44`에 저장합니다.
- **미확정 — 상위 field writer와 간접 연관성.** `0x00acd708+0x11c`를 쓰는 초기화 루틴과 raw-I/O 값이 해당 field를 간접적으로 결정하는지는 아직 확인되지 않았습니다. 그러므로 이번 결과는 “Hardlock 직접 실패”가 아니라 “상위 객체 초기화/전달 경로의 null receiver”로 분류하며, 4th 물리 I/O 응답 의미를 새로 확정하지 않습니다.

* **Confirmed — faulting instruction and receiver.** `20260903-012922-258.jsonl` reproduces a read AV at `0x00434137` on thread `18688`, reading `0x00000014`. The runtime bytes are `8b 48 14` (`mov ecx, [eax+0x14]`), and the AV registers are `EAX=0`, `ECX=0`.
* **Confirmed — runtime target chain.** The stack direct-call candidate `0x00402275` contains runtime bytes `e9 9c 1e 03 00`, which jumps to `0x00434116`. `0x00434116` stores `ECX` in a local and then reads `[EAX+0x14]`; the immediate fault cause is therefore a null object receiver.
* **Confirmed — the immediate caller passes null.** Runtime callsite `0x00417da4` executes `mov [EBP-8], ECX`, then `mov ECX, [EBP-8]`, and calls `0x00402275`. `av_caller_frame` records this caller local as `0x00000000`. The callee return address is `0x00417dc7`, and the caller return address is `0x0041a6a4`.
* **Confirmed — the field one level above is zero.** The runtime window near `0x0041a684` contains `mov ECX, [EBP-0x118]`, `mov ECX, [ECX+0x11c]`, and `call 0x00402298`. `av_outer_frame` records `[outer EBP-0x118] = 0x00acd708` and `[(0x00acd708)+0x11c] = 0x00000000`. This is the nearest confirmed source of the null passed to `0x00417da4`.
* **Confirmed — Hardlock success boundary is separate from the direct AV.** Separate run `20260903-010325-894.jsonl` records the `0x450` response `0100fafa0010` with `EAX=1`, and `20260903-010211-910.jsonl` records the final `0x44c` with a 256-byte output and `EAX=1`. Current evidence therefore does not support a null or failed `DeviceIoControl` return as the direct AV cause.
* **Confirmed — raw-I/O return values and storage.** `20260903-010808-683.jsonl` records `0x0103 -> 0x80`, `0x0104 -> 0x80`, and `0x0105 -> 0x00`; each return site stores the value into original-object offsets `+0xb3c`, `+0xb40`, and `+0xb44`.
* **Unresolved — upper-field writer and indirect relation.** The initializer that writes `0x00acd708+0x11c`, and whether raw-I/O state indirectly determines that field, remain unconfirmed. The result is therefore classified as an upstream object-initialization/null-receiver path, not a direct Hardlock failure, and does not confirm a physical 4th I/O-board response.

관련 진단 설계와 작업 지시서는 [Task 145 설계](../design/20260903-145-ez2dj4th-av-null-context.md)와 [Task 145 작업 지시서](../work-orders/20260903-145-ez2dj4th-av-null-context.md)에 둡니다.

The related diagnostic design and work order are [Task 145 design](../design/20260903-145-ez2dj4th-av-null-context.md) and [Task 145 work order](../work-orders/20260903-145-ez2dj4th-av-null-context.md).

## 2026-09-03 null field writer trace

- **확인됨 — write watch 준비와 초기값.** `20260903-014526-938.jsonl` 및 `20260903-014716-040.jsonl`에서 `image_base + 0x006cd824 = 0x00acd824`에 대한 x86 `DR3` 4-byte write watch가 `prepared=true`로 설정됐고, ready 시점 field 값은 `0x00000000`이었습니다.
- **확인됨 — AV 전 writer hit 없음.** 두 실행 모두 `null_context_field_writer_hit`가 0회였고, 동일한 `0x00434137 / 0xc0000005` AV가 발생했습니다. child-exit boundary에서도 field current 값은 0이었습니다. 이는 watch 설치 후 해당 4-byte 범위에 쓰기가 관찰되지 않았다는 뜻입니다.
- **확인됨 — 고정 absolute reference 없음.** `20260903-014716-040.jsonl`의 읽기 전용 image scan은 field `0x00acd824`에 대해 `matches=0`을 기록했습니다. 고정 주소를 직접 피연산자로 갖는 runtime image reference는 이 scan에서 발견되지 않았습니다.
- **확인됨 — 기존 breakpoint와 공존.** `20260903-014819-394.jsonl`은 `--slot-writer-trace`와 새 `DR3` watch를 동시에 사용했으며, 기존 slot writer `0x00aefe62` hit(`EAX=0x00b17b00`)를 정상 기록했습니다. 두 breakpoint 설정의 충돌은 관찰되지 않았습니다.
- **미확정 — writer 시점과 접근 경로.** field가 watch 설치 전에 이미 0이었거나, 간접 객체 경로로 접근될 가능성이 있습니다. 이는 writer가 존재하지 않는다는 증명이 아닙니다. 동일 field의 read/access watch로 최초 사용 지점과 상위 객체 공급 경로를 확인해야 합니다.

* **Confirmed — write-watch setup and initial value.** `20260903-014526-938.jsonl` and `20260903-014716-040.jsonl` set an x86 `DR3` four-byte write watch at `image_base + 0x006cd824 = 0x00acd824` with `prepared=true`; the field was `0x00000000` when the watch was armed.
* **Confirmed — no writer hit before the AV.** Both runs recorded zero `null_context_field_writer_hit` events and produced the same `0x00434137 / 0xc0000005` AV. The field current value was also zero at the child-exit boundary. This means no write to the watched four-byte range was observed after arming.
* **Confirmed — no fixed absolute reference.** The read-only image scan in `20260903-014716-040.jsonl` reported `matches=0` for field `0x00acd824`. No runtime-image reference with that fixed address as an immediate operand was found by this scan.
* **Confirmed — coexistence with existing breakpoints.** `20260903-014819-394.jsonl` used `--slot-writer-trace` together with the new `DR3` watch and recorded the existing slot writer hit at `0x00aefe62` with `EAX=0x00b17b00`. No collision between the breakpoint configurations was observed.
* **Unresolved — writer timing and access path.** The field may already have been zero before the watch was armed, or may be accessed through an indirect object path. This does not prove that no writer exists. A read/access watch on the same field is required to identify the first use and upper-object supply path.

관련 설계와 작업 지시서는 [Task 146 설계](../design/20260903-146-ez2dj4th-null-field-writer-trace.md)와 [Task 146 작업 지시서](../work-orders/20260903-146-ez2dj4th-null-field-writer-trace.md)에 둡니다.

The related design and work order are [Task 146 design](../design/20260903-146-ez2dj4th-null-field-writer-trace.md) and [Task 146 work order](../work-orders/20260903-146-ez2dj4th-null-field-writer-trace.md).

## 2026-09-03 null field access trace

- **확인됨 — access watch와 최초 접근.** `20260903-015902-887.jsonl`에서 `image_base + 0x006cd824 = 0x00acd824`에 대한 `DR3` read/write access watch가 `prepared=true`로 설정되었습니다. 첫 hit는 thread `4772`에서 발생했으며 field 값은 before/after 모두 `0x00000000`이었습니다.
- **확인됨 — field read site.** 첫 hit의 post-access EIP는 `0x0041a69f`이고 runtime code window는 `0x0041a699`의 `mov ECX, [ECX+0x11c]`와 이어지는 `call 0x00402298`를 포함합니다. 이 명령이 `0x00acd824` field를 읽은 지점으로 확인됩니다.
- **확인됨 — read 직후 null 전달.** 같은 access hit에서 `ECX=0`이었고, 같은 thread가 곧바로 `0x00434137`에서 `0x00000014` read AV를 발생시켰습니다. `av_outer_frame`은 outer object `0x00acd708`의 `+0x11c` field가 여전히 0임을 기록했습니다.
- **확인됨 — slot writer와 공존.** `20260903-020004-160.jsonl`은 기존 slot-writer `DR0–DR2`와 field access `DR3`를 함께 arm했고, 같은 thread에서 slot-writer hit와 field access hit를 모두 정상 기록했습니다.
- **판정 — read site와 null 전달 순서 확인.** field는 0으로 읽힌 뒤 `ECX=0`인 상태로 다음 호출에 전달되고, 동일 thread에서 null receiver AV가 발생합니다. 이는 Hardlock 반환 경로가 이 field를 직접 채우지 못했다는 실행 증거이지만, field가 왜 0인지와 어떤 HLE 경계가 상위 객체를 초기화해야 하는지는 미확정입니다. 따라서 field 값을 직접 주입하거나 Hardlock 응답값을 추측하는 수정은 보류합니다.

* **Confirmed — access watch and first access.** `20260903-015902-887.jsonl` prepared a `DR3` read/write access watch for `image_base + 0x006cd824 = 0x00acd824` with `prepared=true`. The first hit occurred on thread `4772`, and the field value was `0x00000000` both before and after the access.
* **Confirmed — field read site.** The first hit's post-access EIP was `0x0041a69f`, and the runtime code window includes `mov ECX, [ECX+0x11c]` at `0x0041a699` followed by `call 0x00402298`. This identifies the instruction that read the field at `0x00acd824`.
* **Confirmed — null propagation immediately after the read.** The same access hit had `ECX=0`, and the same thread then raised the `0x00000014` read AV at `0x00434137`. `av_outer_frame` records that the `+0x11c` field of outer object `0x00acd708` was still zero.
* **Confirmed — coexistence with the slot writer.** `20260903-020004-160.jsonl` armed the existing slot-writer `DR0`–`DR2` watches together with field access `DR3`, and recorded both the slot-writer hit and field-access hit on the same thread.
* **Classification — read site and null-propagation order confirmed.** The field is read as zero, passed onward with `ECX=0`, and the same thread reaches the null-receiver AV. This is execution evidence that the Hardlock return path does not directly populate this field, but why the field is zero and which HLE boundary should initialize the upper object remain unresolved. Direct field injection and guessed Hardlock responses are therefore deferred.

관련 설계와 작업 지시서 및 실행 결과는 [Task 147 설계](../design/20260903-147-ez2dj4th-null-field-access-trace.md), [Task 147 작업 지시서](../work-orders/20260903-147-ez2dj4th-null-field-access-trace.md), [Task 147 작업 로그](../work-logs/20260903-147-ez2dj4th-null-field-access-trace.md)에 둡니다.

The related design, work order, and execution record are [Task 147 design](../design/20260903-147-ez2dj4th-null-field-access-trace.md), [Task 147 work order](../work-orders/20260903-147-ez2dj4th-null-field-access-trace.md), and [Task 147 work log](../work-logs/20260903-147-ez2dj4th-null-field-access-trace.md).

## 2026-09-03 upper-object allocation trace

- **확인됨 — 객체 context.** `20260903-023341-871.jsonl`의 첫 field access hit는 `[EBP-0x118] = 0x00acd708`, 객체 field 주소 `0x00acd824`, field 값 `0`을 기록했습니다. 객체 주소는 관찰 image base 기준 `image_base + 0x006cd708`인 image-resident 주소입니다.
- **확인됨 — 관찰된 allocator와 불일치.** 같은 실행에서 `LocalAlloc`·`VirtualAlloc` return hit 34,537건을 관찰했지만 `0x00acd708`과 일치하는 `EAX`는 없었고, field event의 `allocation_return_match`도 `false`였습니다. 상세 return event는 256건으로 제한했지만 비교용 주소 집합은 계속 갱신했습니다.
- **미확정 — HeapAlloc 범위.** `HeapAlloc` export는 forwarded 상태여서 watch가 설치되지 않았습니다. 따라서 `HeapAlloc`이 객체를 반환했는지는 이 실행으로 판정할 수 없습니다.
- **판정 — pre-existing/other origin 범위.** `0x00acd708`은 관찰된 두 Win32 allocator의 반환값이 아니며, 관찰 경계에서 이미 존재하는 정적 객체이거나 다른 공급 경로에서 온 것으로 분류합니다. 이는 `[EBP-0x118]`에 값을 쓴 instruction을 식별한 결과가 아니므로, 다음에는 해당 stack-local assignment 또는 caller를 추적해야 합니다.

관련 설계, 작업 지시서 및 작업 로그는 [Task 148 설계](../design/20260903-148-ez2dj4th-object-allocation-trace.md), [Task 148 작업 지시서](../work-orders/20260903-148-ez2dj4th-object-allocation-trace.md), [Task 148 작업 로그](../work-logs/20260903-148-ez2dj4th-object-allocation-trace.md)에 둡니다.

* **Confirmed — object context.** The first field-access hit in `20260903-023341-871.jsonl` recorded `[EBP-0x118] = 0x00acd708`, object-field address `0x00acd824`, and field value zero. The object address is image-resident at `image_base + 0x006cd708` for the observed image base.
* **Confirmed — mismatch with observed allocators.** The same run observed 34,537 `LocalAlloc` and `VirtualAlloc` return hits, but no `EAX` matched `0x00acd708`; the field event also reported `allocation_return_match=false`. Detailed return events were bounded at 256 while the comparison address set continued to update.
* **Unresolved — HeapAlloc scope.** The `HeapAlloc` export was forwarded, so no watch was installed. This run cannot determine whether `HeapAlloc` returned the object.
* **Classification — pre-existing/other origin within scope.** `0x00acd708` was not returned by the two observed Win32 allocators and is classified as a pre-existing static object or a value from another supply path at this observation boundary. This does not identify the instruction that populated `[EBP-0x118]`; the next step is to trace that stack-local assignment or its caller.

The related Task 148 design, work order, and work log are [Task 148 design](../design/20260903-148-ez2dj4th-object-allocation-trace.md), [Task 148 work order](../work-orders/20260903-148-ez2dj4th-object-allocation-trace.md), and [Task 148 work log](../work-logs/20260903-148-ez2dj4th-object-allocation-trace.md).

## 2026-09-03 object source stack-slot trace

- **확인됨 — 고정 stack-slot watch 설치.** `20260903-025152-528.jsonl`에서 `DR2` 4-byte write watch가 configured slot `0x001afcf0`에 `prepared=true`로 설치되었습니다. source hit 61건이 기록되었습니다.
- **확인됨 — 고정 주소와 target frame 불일치.** 준비 시점 configured slot 값은 `0x001afd24`였고, 61건 모두 `stack_slot_matches_target=false`, `frame_slot_matches_target=false`였습니다. `target_matches=0`으로 target object `0x00acd708`과 일치한 공급 write는 관찰되지 않았습니다.
- **확인됨 — 고정 주소의 재사용.** hit context 중 다수는 `0x001afcf0`이 다른 OS/runtime frame의 stack 또는 `ESP`로 사용되는 쓰기였습니다. 따라서 이전 실행에서의 absolute stack address를 다음 실행의 object source 주소로 재사용할 수 없습니다.
- **확인됨 — baseline frame은 기존과 동일.** source watch를 끈 `20260903-025218-345.jsonl`에서는 field access 1건이 `EBP=0x001afe08`, `[EBP-0x118]=0x001afcf0`, object `0x00acd708`, field `0x00000000`을 기록한 뒤 `0x00434137` AV로 진행했습니다.
- **미확정 — object 공급 instruction.** 고정 stack-slot watch는 source mechanism과 주소 drift를 확인했지만, target object를 `[EBP-0x118]`에 공급하는 instruction은 확인하지 못했습니다. 다음 진단은 runtime field-access 함수의 진입 경계에서 현재 `EBP`를 계산해 동적으로 watch를 설치해야 합니다.

* **Confirmed — fixed stack-slot watch setup.** `20260903-025152-528.jsonl` prepared a `DR2` four-byte write watch at configured slot `0x001afcf0` with `prepared=true` and recorded 61 source hits.
* **Confirmed — fixed address does not match the target frame.** The configured slot contained `0x001afd24` when armed, and all 61 hits reported `stack_slot_matches_target=false` and `frame_slot_matches_target=false`. `target_matches=0`, so no write supplying target object `0x00acd708` was observed.
* **Confirmed — fixed-address reuse.** Many hit contexts used `0x001afcf0` as another OS/runtime frame's stack or as `ESP`. An absolute stack address from one run cannot be reused as the object-source address in the next run.
* **Confirmed — baseline frame remains unchanged.** Without the source watch, `20260903-025218-345.jsonl` recorded one field access with `EBP=0x001afe08`, `[EBP-0x118]=0x001afcf0`, object `0x00acd708`, and field `0x00000000`, followed by the `0x00434137` AV.
* **Unresolved — object-supply instruction.** The fixed stack-slot watch validated the source mechanism and address drift but did not identify the instruction that supplies the target object to `[EBP-0x118]`. The next diagnostic should derive the current `EBP` at the runtime field-access function boundary and install the watch dynamically.

The related Task 149 design, work order, and work log are [Task 149 design](../design/20260903-149-ez2dj4th-object-source-trace.md), [Task 149 work order](../work-orders/20260903-149-ez2dj4th-object-source-trace.md), and [Task 149 work log](../work-logs/20260903-149-ez2dj4th-object-source-trace.md).

## 2026-09-03 dynamic object source boundary trace

- **확인됨 — runtime boundary.** baseline field-access run `20260903-030307-249.jsonl`의 runtime scan은 field-read anchor `0x0041a699` 앞에서 prologue `0x0041a649`와 prologue 직후 boundary `0x0041a64c`를 확인했습니다. entry 준비 시점에는 보호 stub가 runtime code를 아직 복호화하지 않아 같은 scan이 실패했으므로, 확인된 boundary RVA `0x001a64c`를 다음 run의 `DR0` anchor로 사용했습니다.
- **확인됨 — dynamic frame slot.** `20260903-030421-317.jsonl`에서 `DR0` boundary hit는 `EBP=0x001afe08`을 기록했고, 현재 frame slot `EBP-0x118=0x001afcf0`에 `DR2` write watch를 동적으로 설치했습니다.
- **확인됨 — upper-object supply instruction.** 같은 run의 두 번째 source hit는 slot 값 `0x00acd708`, `frame_slot_matches_target=true`를 기록했습니다. runtime window는 `0x0041a668` 시작의 `mov [EBP-0x118], ECX`와 post-EIP `0x0041a66e`를 나타내며, 대입 시 `ECX=0x00acd708`이었습니다.
- **확인됨 — field zero와 fault 순서.** source hit 뒤 같은 thread에서 `0x00acd824` field access가 발생했고 field 값은 `0x00000000`이었습니다. 이후 `0x00434137`에서 `0x00000014` read AV가 재현되었습니다.
- **미확정 — field initializer.** 상위 객체 pointer 공급 instruction은 확인했지만 `0x00acd708+0x11c`를 0이 아닌 값으로 초기화하는 instruction과 그 HLE 경계는 확인하지 못했습니다. 직접 field 주입과 Hardlock 응답 변경은 여전히 보류합니다.

* **Confirmed — runtime boundary.** The baseline field-access run `20260903-030307-249.jsonl` scanned runtime bytes before field-read anchor `0x0041a699` and confirmed prologue `0x0041a649` and the post-prologue boundary `0x0041a64c`. The same scan failed during entry preparation because the protection stub had not decrypted runtime code yet, so the confirmed boundary RVA `0x001a64c` was used as the `DR0` anchor for the next run.
* **Confirmed — dynamic frame slot.** In `20260903-030421-317.jsonl`, the `DR0` boundary hit recorded `EBP=0x001afe08` and dynamically installed a `DR2` write watch at the current frame slot `EBP-0x118=0x001afcf0`.
* **Confirmed — upper-object supply instruction.** The second source hit in the same run recorded slot value `0x00acd708` with `frame_slot_matches_target=true`. Its runtime window identifies `mov [EBP-0x118], ECX` starting at `0x0041a668`, post-EIP `0x0041a66e`, with `ECX=0x00acd708` at assignment.
* **Confirmed — field-zero and fault order.** The same thread accessed field `0x00acd824` after the source hit while the field value was `0x00000000`, then reproduced the `0x00000014` read AV at `0x00434137`.
* **Unresolved — field initializer.** The upper-object pointer supply instruction is confirmed, but the instruction initializing `0x00acd708+0x11c` to a nonzero value and the appropriate HLE boundary remain unknown. Direct field injection and Hardlock-response changes remain deferred.

The related Task 150 design, work order, and work log are [Task 150 design](../design/20260903-150-ez2dj4th-dynamic-object-source-trace.md), [Task 150 work order](../work-orders/20260903-150-ez2dj4th-dynamic-object-source-trace.md), and [Task 150 work log](../work-logs/20260903-150-ez2dj4th-dynamic-object-source-trace.md).

## 2026-09-03 pre-entry field writer trace

- **확인됨 — pre-entry watch 설치.** `20260903-031354-995.jsonl`에서 CREATE_PROCESS/CREATE_THREAD 단계의 `DR3` write watch가 field `0x00acd824`에 설치되었고 early thread arm event가 기록되었습니다.
- **확인됨 — early writer 미관찰.** initial breakpoint 이전 구간에서 `null_context_field_writer_early_hit`는 0건이었습니다. 이 관찰 범위에서는 보호 stub 또는 pre-entry 초기화가 field에 write하지 않았습니다.
- **확인됨 — 이후 source/access 순서 유지.** 같은 실행에서 object source boundary `0x0041a64c` 1건, dynamic source hit 2건과 target match 1건, field access 1건이 기록되었습니다. field 값은 `0x00000000`이고 이후 `0x00434137` read AV가 발생했습니다.
- **미확정 — field 결정 경로.** early writer가 없다는 결과만으로 field가 항상 미초기화라고 확정할 수는 없습니다. 다른 주소의 write, 간접 계산, 또는 관찰 boundary 밖의 초기화 가능성은 남아 있습니다.

* **Confirmed — pre-entry watch setup.** `20260903-031354-995.jsonl` installed the `DR3` write watch for field `0x00acd824` during CREATE_PROCESS/CREATE_THREAD handling and recorded early thread-arm events.
* **Confirmed — no early writer observed.** `null_context_field_writer_early_hit` occurred zero times before the initial breakpoint. Within this observation scope, the protection stub and pre-entry initialization did not write the field.
* **Confirmed — later source/access order preserved.** The same run recorded one object-source boundary hit at `0x0041a64c`, two dynamic source hits with one target match, and one field access. The field was `0x00000000`, followed by the `0x00434137` read AV.
* **Unresolved — field determination path.** The absence of an early writer does not prove the field is always uninitialized. A write at another address, indirect calculation, or initialization outside the observed boundary remains possible.

The related Task 151 design, work order, and work log are [Task 151 design](../design/20260903-151-ez2dj4th-pre-entry-field-writer-trace.md), [Task 151 work order](../work-orders/20260903-151-ez2dj4th-pre-entry-field-writer-trace.md), and [Task 151 work log](../work-logs/20260903-151-ez2dj4th-pre-entry-field-writer-trace.md).

## 2026-09-03 runtime +0x11c direct-reference scan

- **확인됨 — 복호화된 `.text` 스캔 성공.** `20260903-032038-359.jsonl`의 첫 field access 시점에 image base `0x00400000`의 runtime `.text` 범위 `0x00401000`–`0x004dc021` (`RVA 0x001000`, virtual size `0x000db022`)를 모두 읽었습니다. `readable=true`, `bytes_copied=897058`입니다.
- **확인됨 — 직접 displacement 후보 존재.** displacement `0x0000011c`를 가진 syntactic 후보는 23개였고 read 분류 16개, write 분류 4개, 기타 분류 3개였습니다. write 후보는 runtime 주소 `0x0040fdbd` (`RVA 0x0000fdbd`, `89 81`), `0x0040fde1` (`RVA 0x0000fde1`, `c7 81`), `0x0041825f` (`RVA 0x0001825f`, `c7 80`), `0x0041dbd3` (`RVA 0x0001dbd3`, `89 8a`)입니다.
- **확인됨 — 알려진 read site 포함.** 기존 field read instruction `0x0041a699` (`RVA 0x0001a699`, `8b 89`)가 16개 read 후보 중 하나로 다시 확인되었습니다. 실제 access hit는 여전히 field `0x00acd824`를 `0x00000000`으로 읽었고 `0x00434137` null receiver AV로 이어졌습니다.
- **추정 — 후보 writer와 target object의 관계.** 네 write 후보는 `+0x11c`를 사용하는 명령이라는 점만 확인된 것이며, 각 명령이 실제 object `0x00acd708`를 receiver로 실행되었는지는 확인되지 않았습니다. 동일 offset을 사용하는 다른 object type일 가능성이 있습니다. 기타 후보에는 전체 instruction decoder 없이 수집된 arithmetic/branch 형태가 포함되므로 field access 증거로 사용하지 않습니다.
- **미확정 — 실제 초기화 writer.** 첫 access 이전에 네 write 후보 중 하나가 `0x00acd708 + 0x11c`를 썼는지, 또는 field가 다른 간접 복사 경로로 정해지는지는 아직 미확정입니다. 다음 단계는 네 후보의 실행 여부와 receiver register/target address를 제한적으로 추적하는 것입니다.

* **Confirmed — decrypted `.text` scan succeeded.** At the first field-access point in `20260903-032038-359.jsonl`, the complete runtime `.text` range at image base `0x00400000`, from `0x00401000` through `0x004dc021` (`RVA 0x001000`, virtual size `0x000db022`), was readable. The event reports `readable=true` and `bytes_copied=897058`.
* **Confirmed — direct-displacement candidates exist.** There were 23 syntactic candidates using displacement `0x0000011c`: 16 classified as reads, four as writes, and three as other. The write candidates are runtime addresses `0x0040fdbd` (`RVA 0x0000fdbd`, `89 81`), `0x0040fde1` (`RVA 0x0000fde1`, `c7 81`), `0x0041825f` (`RVA 0x0001825f`, `c7 80`), and `0x0041dbd3` (`RVA 0x0001dbd3`, `89 8a`).
* **Confirmed — known read site included.** The existing field-read instruction at `0x0041a699` (`RVA 0x0001a699`, `8b 89`) was rediscovered as one of the 16 read candidates. The runtime access hit still read field `0x00acd824` as `0x00000000` and then reached the `0x00434137` null-receiver AV.
* **Inferred — relationship between writer candidates and target object.** The four write candidates are confirmed only as instructions using displacement `+0x11c`; it is not confirmed that any executed with object `0x00acd708` as the receiver. They may belong to other object types sharing the same offset. The other candidates include arithmetic/branch forms collected without a full instruction decoder and are not treated as field-access evidence.
* **Unresolved — actual initialization writer.** It remains unresolved whether one of the four write candidates wrote `0x00acd708 + 0x11c` before the first access, or whether the field is established through another indirect copy path. The next step is a bounded trace of candidate execution and receiver/target addresses.

The related Task 152 design, work order, and work log are [Task 152 design](../design/20260903-152-ez2dj4th-runtime-field-reference-scan.md), [Task 152 work order](../work-orders/20260903-152-ez2dj4th-runtime-field-reference-scan.md), and [Task 152 work log](../work-logs/20260903-152-ez2dj4th-runtime-field-reference-scan.md).
