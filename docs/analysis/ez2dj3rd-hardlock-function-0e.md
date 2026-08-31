# ez2dj3rd Hardlock Function 0x0e 분석

## 범위

이 문서는 저장소에 포함되지 않은 합법적 사용자 자산으로 `ez2dj3rd` 원본 실행 파일을 bounded 실행하고, injected runtime의 계측으로 확인한 Hardlock 경계를 기록합니다. 원본 바이너리, HDD 자산, dongle dump, 전체 바이트 열은 기록하지 않습니다.

This document records the Hardlock boundary observed by bounded execution of the user-supplied `ez2dj3rd` original executable through the injected runtime. Original binaries, HDD assets, dongle dumps, and complete byte sequences are not stored in the repository.

## 확인됨 / Confirmed

- 3rd 원본 실행 파일의 보호 entry 경로에는 `KERNEL32.dll!GetProcAddress` IAT 슬롯이 두 개 있습니다. 하나만 수정하면 동적 API 해석이 불완전하므로 두 슬롯을 모두 runtime resolver에 연결해야 합니다.
- all-slot 연결 이후 `CreateFileA` 호출에서 `\\.\NTICE`와 `\\.\FEnteDev`를 관찰했습니다. 3rd 프로파일의 synthetic device path는 `\\.\FEnteDev`로 설정했습니다. `NTICE`의 정확한 역할은 아직 확정하지 않았습니다.
- synthetic `FEnteDev` handle을 연결한 계측에서 다음 device request 경계를 관찰했습니다: `0x9c402468`, `0x9c402450`, `0x9c40244c`, `0x9c402458`.
- `0x9c40244c`는 256바이트 in-place API descriptor 경계입니다. 관찰된 API version은 `0x4703`이며 초기 `Function`은 `0`, 이후 `Function 6` 요청이 관찰되었습니다.
- `0x9c402450`은 6바이트 입출력 버퍼를 사용하는 하위 응답 경계이며, 계측된 synthetic 응답 경로에서 `FA FA` marker 검사가 나타났습니다.
- `0x9c402458`은 264바이트 입출력 버퍼를 사용합니다. 앞의 256바이트는 API descriptor이고 `Function`은 `0x0e`, 뒤 8바이트는 호출마다 달라지는 암호 블록입니다.

- The 3rd original executable has two `KERNEL32.dll!GetProcAddress` IAT slots on the protected entry path. Routing only one leaves dynamic API resolution incomplete, so both slots must point to the runtime resolver.
- After all-slot routing, `CreateFileA` requests for `\\.\NTICE` and `\\.\FEnteDev` were observed. The 3rd profile now uses `\\.\FEnteDev` as its synthetic device path. The exact role of `NTICE` remains unconfirmed.
- Instrumentation with a synthetic `FEnteDev` handle observed device request boundaries at `0x9c402468`, `0x9c402450`, `0x9c40244c`, and `0x9c402458`.
- `0x9c40244c` is a 256-byte in-place API descriptor boundary. The observed API version is `0x4703`; the initial `Function` is `0`, followed by observed `Function 6` requests.
- `0x9c402450` is a lower-level response boundary using a six-byte input/output buffer. The instrumented synthetic response path exposed an `FA FA` marker check.
- `0x9c402458` uses a 264-byte input/output buffer. Its first 256 bytes are an API descriptor with `Function 0x0e`; the final eight bytes are an encrypted block that changes per call.

## 추정 / Inferred

- `FEnteDev`는 이 실행 파일이 기대하는 Hardlock 장치 이름으로 보는 것이 타당합니다. `NTICE`는 별도의 anti-debug 또는 보호 환경 확인용 장치 조회일 가능성이 있으나, 현재 증거만으로 확정하지 않습니다.
- `0x9c402458`은 공개적으로 알려진 Hardlock API의 일반 descriptor 호출과 별개인 암호 응답 경계로 보입니다. 따라서 1st SE의 LPTDI 응답 마스크를 그대로 재사용하는 것은 계약상 안전하지 않습니다.

- `FEnteDev` is reasonably treated as the Hardlock device name expected by this executable. `NTICE` may be a separate anti-debug or protection-environment query, but the current evidence does not establish that role.
- `0x9c402458` appears to be a cryptographic response boundary separate from the ordinary public Hardlock descriptor calls. Reusing the 1st SE LPTDI response mask as-is is therefore not contract-safe.

## 미확정 / Unresolved

- 3rd `Function 0x0e` 요청에 대응하는 유효한 8바이트 응답을 아직 확보하지 못했습니다.
- 그 응답을 생성하는 세 개의 16비트 Hardlock seed도 아직 확인하지 못했습니다.
- 현재 profile의 zero target state는 진단용 placeholder일 뿐이며, 유효한 3rd seed나 응답으로 승격하면 안 됩니다.
- 1st SE의 `LPTDI` 요청 코드와 mask transform은 3rd에서 관찰된 `0x9c402450/44c/458` 계약을 대체할 수 없습니다.

- A valid eight-byte response for the 3rd `Function 0x0e` request has not been obtained.
- The three 16-bit Hardlock seeds that generate that response are also unknown.
- The profile's current zero target state is a diagnostic placeholder and must not be promoted to a valid 3rd seed or response.
- The 1st SE `LPTDI` request codes and mask transform cannot substitute for the `0x9c402450/44c/458` contract observed in the 3rd executable.

## EXE에서 확인한 0x0e 입력 블록 / Function 0x0e input blocks found in the EXE

분석 대상은 크기 `1,216,512`바이트, SHA-256 `E370CA0DEBE58E57784EE72E50A1C3A8811C83555238B9D1A40E1717BE724B4D`인 `EZ2DJ.EXE`입니다. 이 파일의 `.text`는 raw offset과 RVA가 모두 `0x1000`에서 시작합니다.

`20260830-233623-425.vfs.log`에서 관찰한 18개의 `Function 0x0e` 요청은 아래 순서입니다. 각 8바이트 입력은 EXE의 `0x1000 + n * 0x8000` raw offset, 즉 각 32KiB `.text` 구간의 첫 8바이트와 정확히 일치합니다.

*The analyzed `EZ2DJ.EXE` is 1,216,512 bytes with SHA-256 `E370CA0DEBE58E57784EE72E50A1C3A8811C83555238B9D1A40E1717BE724B4D`. Its `.text` section begins at both raw offset and RVA `0x1000`. The 18 observed `Function 0x0e` requests below match the first eight bytes at raw offset `0x1000 + n * 0x8000`, the start of each 32 KiB `.text` chunk.*

| 순서 / Order | Raw offset = RVA | VA | 0x0e 입력 / Input |
|---:|---:|---:|---|
| 0 | `0x001000` | `0x00401000` | `987b20a357ccd4da` |
| 1 | `0x009000` | `0x00409000` | `72d0d80ae8a6f27e` |
| 2 | `0x011000` | `0x00411000` | `2cba42cbe47776f3` |
| 3 | `0x019000` | `0x00419000` | `e2958ca125bd3957` |
| 4 | `0x021000` | `0x00421000` | `73df06d8dd44ecc1` |
| 5 | `0x029000` | `0x00429000` | `c2aae2ade0bd791b` |
| 6 | `0x031000` | `0x00431000` | `6ac19486c6841daa` |
| 7 | `0x039000` | `0x00439000` | `43ae80021ee7955c` |
| 8 | `0x041000` | `0x00441000` | `cf1c1b550ce4365e` |
| 9 | `0x049000` | `0x00449000` | `d1f98e21f5820178` |
| 10 | `0x051000` | `0x00451000` | `3c1a73b33ed13482` |
| 11 | `0x059000` | `0x00459000` | `b3effc2013deb6ea` |
| 12 | `0x061000` | `0x00461000` | `fad8cd1f79da1c85` |
| 13 | `0x069000` | `0x00469000` | `5cdd2be16e707a23` |
| 14 | `0x071000` | `0x00471000` | `2cba42cbe47776f3` |
| 15 | `0x079000` | `0x00479000` | `14299290849c79d7` |
| 16 | `0x081000` | `0x00481000` | `5a6f58a9b2ebac1c` |
| 17 | `0x089000` | `0x00489000` | `5dd33c7c8a27addb` |

**확인됨:** 18회 요청에는 17개의 고유 입력이 있으며 `2cba42cbe47776f3`가 두 번 사용됩니다. 호출 wrapper는 `DeviceIoControl`의 성공 여부를 검사하지만, 반환된 8바이트를 고정 상수와 직접 비교하지 않습니다. 264바이트 packet은 입력과 출력이 같은 주소인 in-place 계약입니다.

**추정:** 32KiB 간격과 `.text` chunk 시작의 일치는 Hardlock 반환값이 각 chunk를 해제하는 암호 재료로 사용됨을 강하게 시사합니다. 디스크의 `.protect`는 보호된 바이트라 정적 역어셈블만으로 반환값을 복원할 수 없습니다.

**미확정:** 분석한 호출부에서는 유효한 dongle 출력을 평문 고정 상수와 비교하는 경로가 확인되지 않았으며, 현재 trace의 output은 synthetic no-op 때문에 input과 동일합니다. 따라서 위 목록은 정확한 입력 목록이지만 유효한 입출력 쌍은 아닙니다. 출력 8바이트를 확정하려면 실제 3rd Hardlock 응답을 한 번 이상 관찰하거나 seed를 복구해야 합니다.

*Confirmed: the 18 calls contain 17 unique inputs, with `2cba42cbe47776f3` used twice. The call wrapper tests `DeviceIoControl` success but does not directly compare the returned eight bytes with a fixed constant. The 264-byte packet is in-place, using the same address for input and output.*

*Inferred: the 32 KiB spacing and exact match at each `.text` chunk start strongly suggest that the Hardlock return value supplies cryptographic material used to unlock each chunk. Static disassembly cannot recover that return value from the protected on-disk `.protect` bytes.*

*Unresolved: the analyzed call path does not compare valid dongle outputs with plaintext fixed constants, and the current synthetic no-op trace leaves output equal to input. The table is therefore an exact input list, not a set of valid input/output pairs. Confirming the eight-byte outputs requires at least one observation from the real 3rd Hardlock or recovery of its seeds.*

## 검증 근거 / Verification evidence

- all-slot resolver 실행: `logs/windows_x86_launcher_probe/ez2dj3rd/20260831-000808-162.jsonl`
- profile path 변경 후 product bounded 실행: `logs/windows_x86_launcher_probe/ez2dj3rd/20260831-000859-972.jsonl`
- Hardlock request 계측: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-233623-425.vfs.log`
- 두 실행 모두 원본 게임 화면 도달이나 유효한 Hardlock 인증 성공을 확인한 증거는 아닙니다.

- All-slot resolver run: `logs/windows_x86_launcher_probe/ez2dj3rd/20260831-000808-162.jsonl`
- Product bounded run after the profile path change: `logs/windows_x86_launcher_probe/ez2dj3rd/20260831-000859-972.jsonl`
- Hardlock request instrumentation: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-233623-425.vfs.log`
- Neither run proves arrival at the original game screen or successful Hardlock authentication.

## 외부 참고 / External references

- 공개 Hardlock API header의 descriptor와 함수 번호: [Certificate-Server Hardlock API header](https://github.com/richschonthal/Certificate-Server/blob/master/Hardlock/api/src/fastapi.h)
- Hardlock envelope의 미문서화 함수 `0x0e`와 8바이트 응답에 대한 공개 분석: [Hardlock envelope discussion](https://forum.exetools.com/showthread.php?mode=hybrid&s=9aae82e35172972d1b99e75d44a5bf54&t=6534)

- Public descriptor and function-number reference: [Certificate-Server Hardlock API header](https://github.com/richschonthal/Certificate-Server/blob/master/Hardlock/api/src/fastapi.h)
- Public discussion of the undocumented `0x0e` envelope and eight-byte response: [Hardlock envelope discussion](https://forum.exetools.com/showthread.php?mode=hybrid&s=9aae82e35172972d1b99e75d44a5bf54&t=6534)

## 후속 런타임 코드 복원 / Follow-up runtime-code reconstruction

**확인됨 — 2026-08-31.** 기존 VFS 로그의 겹치는 `hardlock-code-window`와 `caller_bytes`를 주소 기준으로 합쳐 `0x00a4f008`부터 `0x00a4f167`까지의 실행 코드를 복원했습니다. `caller_bytes`는 `DeviceIoControl` 반환 주소보다 48바이트 앞에서 시작합니다. 복원된 wrapper는 descriptor의 `0x16` word를 8배하여 `0x100 + count * 8` 크기의 임시 버퍼를 만들고, 256바이트 descriptor와 descriptor `0x12`가 가리키는 block 배열을 복사한 뒤 `0x9c402458`을 같은 임시 주소의 in/out buffer로 호출합니다. 따라서 마지막 8바이트는 별도 고정 테이블 비교가 아니라 실제 in-place 응답 경계입니다.

**확인됨 — 2026-08-31.** 새 경량 IOCTL 진입 로그를 사용한 bounded 실행 `20260831-015158-836`에서는 `0x9c402468` 한 번만 관찰된 뒤 종료 코드 5가 발생했습니다. 같은 초기 요청을 실패로 반환한 비교 실행 `20260831-015257-962`는 종료 코드 8이었고 후속 `0x44c/0x458`에는 도달하지 않았습니다. 과거 18회 trace의 synthetic output은 요청 buffer를 보존한 진단값이므로 유효한 Hardlock 출력으로 승격할 수 없습니다.

**추정.** 공개된 동시대 분석은 Function `0x0e`가 세 개의 16비트 secret seed를 사용해 8바이트 입력을 8바이트 decryption material로 변환하고 이를 envelope data에 순환 적용한다고 설명합니다. 그러나 1st SE 평문 `.text`를 기준으로 한 단순 반복 XOR 통계 복원은 일관된 x86 평문을 만들지 못했습니다. 따라서 이 EXE에 대한 XOR 적용 위치·방식이나 seed를 통계 후보만으로 확정하지 않습니다.

**미확정.** EXE에는 18개 challenge가 존재하지만 유효 response나 세 seed를 평문 상수로 비교하는 경로는 확인되지 않았습니다. 합법적으로 보유한 실제 3rd Hardlock의 18개 응답, 알려진 seed, 또는 이미 검증된 한 개 이상의 입력/출력 쌍이 없으면 EXE 단독 분석으로 완전한 유효 입출력 표를 확정할 수 없습니다.

*Confirmed — 2026-08-31. Overlapping `hardlock-code-window` and `caller_bytes` records were merged by address, reconstructing executed code from `0x00a4f008` through `0x00a4f167`. A `caller_bytes` window begins 48 bytes before the `DeviceIoControl` return address. The wrapper multiplies the descriptor word at `0x16` by eight, allocates `0x100 + count * 8` bytes, copies the 256-byte descriptor and the block array referenced at descriptor offset `0x12`, then calls `0x9c402458` with the temporary buffer as both input and output. The final eight bytes are therefore a real in-place response boundary, not a fixed-table comparison.*

*Confirmed — 2026-08-31. A bounded run with the new lightweight IOCTL-entry log, `20260831-015158-836`, observed only one `0x9c402468` request before exit code 5. Returning failure for the same initialization request in comparison run `20260831-015257-962` produced exit code 8 and did not reach `0x44c/0x458`. The synthetic outputs in the earlier 18-call trace preserved request buffers and cannot be promoted to valid Hardlock outputs.*

*Inferred. Contemporary public analysis describes Function `0x0e` as using three secret 16-bit seeds to transform an eight-byte input into eight bytes of decryption material cycled over envelope data. A simple repeating-XOR statistical recovery using the 1st SE plaintext `.text` did not produce consistent x86 plaintext. This analysis therefore does not claim an XOR application point, algorithm variant, or seed from statistical candidates.*

*Unresolved. The executable supplies 18 challenges but no observed path compares valid responses or the three seeds against plaintext constants. A complete valid input/output table cannot be established from this executable alone without the 18 responses from a legally owned 3rd Hardlock, known seeds, or at least one independently verified input/output pair.*

## Descriptor key material 진단 준비 / Descriptor key-material diagnostic readiness

**확인됨 — 공개 구조 및 구현 검증.** 공개 packed 32-bit `HL_API` 구조에서 `ID_Ref[8]`과 `ID_Verify[8]`은 각각 descriptor offset `0x24`, `0x2c`에 있습니다. 플랫폼 중립 parser와 Windows injected runtime의 bounded marker가 이 두 필드 및 고정 scalar만 읽도록 구현됐습니다. synthetic `0x9c402458` 호출은 기대 marker를 남겼고, 실제로 호출한 일반 LPTDI `0x9c406410`은 descriptor marker를 남기지 않았습니다. 진단은 기존 IOCTL return value, `LastError`, output buffer와 bytes-returned 정책을 변경하지 않습니다.

**확인됨 — 원본 관찰값, 2026-08-31.** 사용자가 지정한 `roms/ez2dj3rd`에서 원본을 두 번 독립 실행하고, mock 응답을 쓰기 전에 guest descriptor `0x00a67290`의 고정 prefix만 읽었습니다. 두 실행 모두 API version `0x4703`, module ID `0x0000`, module address `0x4c51`, block count `0`, function `0x0000`, status `38`, remote `1`, `ID_Ref=478c8b793f201f8a`, `ID_Verify=cc22ae2da344b2a2`로 일치했습니다. 두 ID는 nonzero이며 반복 실행에서 안정적입니다. 이 값은 seed 자체나 유효한 Function `0x0e` 응답이 아니라 seed 복구 제약식의 입력 후보입니다.

**확인됨 — Terminal Services 진행 경계, 2026-08-31.** 동적 resolver 관찰에서 원본은 `WTSQuerySessionInformationA(WTS_CURRENT_SESSION, WTSConnectState)`를 질의했습니다. 동일한 Windows x86 Debug 빌드와 zero-byte-success IOCTL 정책에서 host가 반환한 4바이트 상태 `3`을 그대로 전달한 실행 `20260831-235043-995`는 `0x9c402468`만 기록했고 `0x9c402450`에는 도달하지 않았습니다. 성공한 4바이트 class 4 결과만 활성 상태 `0`으로 바꾼 실행 `20260831-234743-174`는 동일 정책에서 `0x9c402450`을 세 번 기록했습니다. full-size-success 대조 실행 `20260831-234941-553`도 같은 세 번의 `0x450`을 기록했습니다. 따라서 `0x450` 도달은 bytes-returned 길이가 아니라 현대 host session 상태를 원본이 기대하는 active console 상태로 HLE한 결과입니다.

**미확정 — descriptor status와 6바이트 응답.** 공개 header에서 descriptor status `38`은 `TS_DETECTED`이지만, 이번 검증은 guest status word를 직접 바꾸지 않았으므로 그 필드가 독립적인 분기 입력인지는 확정하지 않습니다. 세 active-session 실행 모두 `0x9c402450`까지만 도달했고 `0x9c40244c/458`에는 도달하지 않았습니다. `0x450`의 input/output buffer는 6바이트이며 zero-byte와 full-size buffer-preserving 응답 모두 다음 경계를 통과하지 못했으므로 유효한 driver-written payload가 필요합니다.

*Confirmed — public layout and implementation verification. In the public packed 32-bit `HL_API`, `ID_Ref[8]` and `ID_Verify[8]` begin at descriptor offsets `0x24` and `0x2c`. A platform-neutral parser and bounded Windows injected-runtime marker now read only these fields and fixed scalars. A synthetic `0x9c402458` call emits the expected marker, while an actually issued ordinary LPTDI `0x9c406410` call emits no descriptor marker. The diagnostic does not alter the existing IOCTL return value, `LastError`, output buffer, or bytes-returned policy.*

*Confirmed — original observations, 2026-08-31. The original executable was run independently twice from the user-specified `roms/ez2dj3rd`, and only the fixed prefix of guest descriptor `0x00a67290` was read before any mock response mutation. Both runs produced API version `0x4703`, module ID `0x0000`, module address `0x4c51`, block count `0`, function `0x0000`, status `38`, remote `1`, `ID_Ref=478c8b793f201f8a`, and `ID_Verify=cc22ae2da344b2a2`. Both IDs are nonzero and stable across runs. They are candidate inputs to seed-recovery constraints, not seeds or a valid Function `0x0e` response.*

*Confirmed — Terminal Services progress boundary, 2026-08-31. Dynamic-resolver observation showed that the original calls `WTSQuerySessionInformationA(WTS_CURRENT_SESSION, WTSConnectState)`. With the same Windows x86 Debug build and zero-byte-success IOCTL policy, run `20260831-235043-995` forwarded the host's four-byte state `3`, recorded only `0x9c402468`, and did not reach `0x9c402450`. Run `20260831-234743-174` changed only a successful four-byte class-4 result to active state `0` and recorded `0x9c402450` three times under the same IOCTL policy. Full-size-success comparison run `20260831-234941-553` recorded the same three `0x450` requests. Reaching `0x450` is therefore caused by HLE of the modern host session state to the active-console state expected by the original, not by the bytes-returned length.*

*Unresolved — descriptor status and six-byte response. The public header names descriptor status `38` as `TS_DETECTED`, but this verification did not directly modify the guest status word and therefore does not establish it as an independent branch input. All three active-session observations reached only `0x9c402450`, not `0x9c40244c/458`. The `0x450` input/output buffer is six bytes, and neither a zero-byte nor a full-size buffer-preserving response passed the next boundary, so a valid driver-written payload is required.*

## SMT seed 복구 결과 / SMT seed-recovery result

**확인됨 — 중간 관계식.** 사용자 승인에 따라 공개 GPL 분석 자료는 저장소 밖 임시 디렉터리에서만 참고했고, 저장소에는 source·binary·SMT 중간 파일을 포함하지 않았습니다. MIT 라이선스 Z3 5.1.0으로 `ID_Ref`에서 `ID_Verify`로 이어지는 5단계 8-bit control 값을 bit-vector로 풀었습니다. 유일한 중간 해는 `74 6c 2c 1c f0`였으며, 이 해를 제외한 동일 제약은 `unsat`였습니다.

**확인됨 — seed 해의 비유일성.** 세 seed를 16-bit bit-vector로 모델링하고, seed1/seed2의 네 bit-pair 분포가 각각 4개이며 seed3의 네 nibble이 서로 다르다는 관계를 적용했습니다. 순수 Z3 모델은 고정 후보 검증에 사용했고, 같은 관계의 scalar evaluator와 AVX2 batch evaluator를 교차 검증해 탐색했습니다. 완전 열거를 끝내기 전에 이미 아래 11개 서로 다른 해가 발견됐고, 모두 scalar `shape`와 `relation` 검증을 통과했습니다. `8b33/82fc/6405`와 `b255/9f90/41e2`는 Z3에 세 값을 고정했을 때도 각각 `sat`였습니다.

| seed1 | seed2 | seed3 |
|---:|---:|---:|
| `1ea6` | `85f2` | `2d15` |
| `2ec3` | `c1d3` | `8256` |
| `34c7` | `a51d` | `0768` |
| `8b33` | `82fc` | `6405` |
| `a16e` | `bc58` | `61c4` |
| `aa96` | `c1ce` | `eb96` |
| `ac53` | `a6a5` | `5fea` |
| `b255` | `9f90` | `41e2` |
| `b478` | `dc43` | `23c6` |
| `be03` | `a335` | `3741` |
| `e12b` | `8a67` | `a8ed` |

**미확정 — 실제 dongle seed.** 위 목록은 비유일성을 입증한 **부분 목록**이며 전체 해 목록이 아닙니다. 단일 `ID_Ref`/`ID_Verify` pair만으로 실제 세 seed를 선택할 수 없으므로, 후보 수 세기만을 위한 완전 탐색은 중단했습니다. 실제 seed를 식별하려면 독립적인 두 번째 E-Y-E challenge/response pair 또는 후보별 Function `0x0e` 출력을 원본 실행 경계에서 판별할 수 있는 검증 oracle이 필요합니다. 어느 후보도 실제 dongle seed나 유효한 Function `0x0e` 응답으로 승격하지 않습니다.

*Confirmed — intermediate relation. Under the user's exception, public GPL analysis material was consulted only in a temporary directory outside the repository; no source, binary, or intermediate SMT file is included here. MIT-licensed Z3 5.1.0 found the unique five-byte control solution `74 6c 2c 1c f0` between `ID_Ref` and `ID_Verify`; excluding it makes the same constraints unsatisfiable.*

*Confirmed — non-unique seed solutions. The three seeds were modeled as 16-bit bit-vectors with four occurrences of every seed1/seed2 bit pair and four distinct seed3 nibbles. Pure Z3 was used for fixed-model checks, while cross-checked scalar and AVX2 batch evaluators explored the same relation. The eleven distinct rows above were found before exhaustive enumeration was stopped; all pass scalar shape and relation checks, and two representative rows are independently satisfiable when fixed in Z3.*

*Unresolved — physical-dongle seeds. The table is a partial list proving non-uniqueness, not a complete solution set. One `ID_Ref`/`ID_Verify` pair cannot select the physical seeds, so enumeration solely to count equivalent models was stopped. Identification requires a second independent E-Y-E challenge/response pair or an original-execution oracle that distinguishes candidate Function `0x0e` outputs. No row is promoted to a physical-dongle seed or valid Function `0x0e` response.*

## 0x9c402450 응답 소비와 replay / `0x9c402450` response consumption and replay

**확인됨 — 현재 request shape, 2026-09-01.** exact-size bounded marker를 추가한 zero-byte 실행 `20260901-000336-290`과 full-size 실행 `20260901-000528-846`은 모두 in-place `01 00 00 00 03 00` packet을 세 번 기록했습니다. 따라서 현재 buffer는 little-endian word 세 개 `command=1`, `marker=0`, `result=3`이며 bytes-returned 정책은 입력이나 반복 횟수를 바꾸지 않습니다.

**확인됨 — 소비 코드.** `0x450` 전용 256-step trace `20260901-000702-177`은 반환 주소 `0x00a4f5ad`부터 helper와 상위 caller를 기록했습니다. IOCTL 성공 뒤 `0x00a4f5c4`가 offset 2 word를 읽고 `0x00a4f5c8`에서 `0xFAFA`와 비교합니다. marker가 다르면 AX를 0으로 만들고, 같으면 offset 4 word를 반환합니다. 현재 marker 0은 helper 결과 0이 되어 상위 `0x00a4ec76`의 unsigned `0x033c` 비교에서 낮은 경로를 선택하고 descriptor status `11`을 기록합니다. 과거 caller 해석에서 marker 분기의 방향을 반대로 기술한 부분은 이 실행 trace로 정정했습니다.

**확인됨 — synthetic replay 인과성.** 별도 parser와 기본 비활성 launcher 옵션으로 exact `0x450`에만 과거 synthetic packet `01 00 FA FA 00 10`을 replay한 실행 `20260901-001743-276`은 첫 현재 request 뒤 두 번째 `0x450`과 `0x9c40244c` descriptor 요청에 도달했고, 다음 cycle에서도 다시 `0x44c`에 도달했습니다. descriptor는 API version `0x4703`, module address `0x4c51`, Function `0`, Status `0`이었습니다. 따라서 marker `FAFA`와 result `0x1000`을 반환하는 6바이트 replay가 `0x44c` 도달에 인과적입니다.

**미확정 — 실제 driver payload와 다음 descriptor 응답.** replay 값은 과거 synthetic buffer-preserving 계측에서 얻은 oracle이며 실제 dongle이나 driver가 반환한 값이 아닙니다. 실행은 `0x44c`까지 진행했지만 `0x458`에는 도달하지 않았습니다. 현재 다음 경계는 256바이트 Function 0 descriptor의 driver-written response이며, Task 107 seed 후보와 Function `0x0e` 응답은 계속 미확정입니다.

*Confirmed — current request shape, 2026-09-01. Exact-size bounded markers in zero-byte run `20260901-000336-290` and full-size run `20260901-000528-846` each recorded the in-place packet `01 00 00 00 03 00` three times. The current buffer is therefore three little-endian words, `command=1`, `marker=0`, and `result=3`; the bytes-returned policy changes neither input nor retry count.*

*Confirmed — consumer code. The 256-step `0x450`-filtered trace `20260901-000702-177` followed return address `0x00a4f5ad` through the helper and upper caller. After IOCTL success, `0x00a4f5c4` loads the word at offset 2 and `0x00a4f5c8` compares it with `0xFAFA`. A mismatch returns zero in AX, while a match returns the word at offset 4. The current zero marker therefore produces helper result zero, selects the below-`0x033c` unsigned branch at `0x00a4ec76`, and writes descriptor status `11`. This execution trace corrects the previously reversed interpretation of the marker branch.*

*Confirmed — synthetic replay causality. With a separate parser and default-off launcher option, run `20260901-001743-276` replayed the historical synthetic packet `01 00 FA FA 00 10` only for exact `0x450`. It advanced from the first current request to a second `0x450` and a `0x9c40244c` descriptor request, then reached `0x44c` again in the next cycle. The descriptor had API version `0x4703`, module address `0x4c51`, Function `0`, and Status `0`. Returning marker `FAFA` and result `0x1000` is therefore causal for reaching `0x44c`.*

*Unresolved — physical-driver payload and next descriptor response. The replay bytes are an oracle from historical synthetic buffer-preserving instrumentation, not a response observed from a physical dongle or driver. Execution reached `0x44c` but not `0x458`. The next boundary is the driver-written response for the 256-byte Function-0 descriptor; Task 107 seed candidates and Function `0x0e` responses remain unresolved.*

## 0x9c40244c descriptor tail 소비 / `0x9c40244c` descriptor-tail consumption

**확인됨 — 현재 tail과 소비 분기, 2026-09-01.** exact 256바이트 descriptor marker와 `0x44c` 전용 post-IOCTL trace를 연결한 실행 `20260901-003431-926`에서 Function 0 request의 offset `0xfe` tail word는 `0x0000`이었습니다. IOCTL은 반환 주소 `0x00a4ef96`에서 성공하고 buffer와 256 bytes returned를 유지했습니다. 상위 helper는 `0x00a4ecd2`에서 성공을 검사한 뒤 `0x00a4ed17`에서 descriptor byte `+0xfe`를 읽고 `0x00a4ed1d`에서 0인지 검사합니다. 현재 0 경로는 handle을 닫고 전역 handle을 `0xffffffff`로 되돌린 뒤 status 0으로 반환합니다.

**확인됨 — synthetic tail 인과성.** 기본 비활성 `--device-mock-hardlock-44c-tail` 옵션은 user-supplied 16-bit word를 exact-size Function 0 `0x9c40244c` output의 마지막 두 바이트에만 씁니다. Task 109 replay와 synthetic `tail=0x0001`을 함께 적용한 실행 `20260901-004347-276`은 첫 Function 0 request에서 `tail_word=0x0000`을 기록한 뒤, Function 6 `0x44c` 30회와 Function `0x0e` `0x458` 30회에 도달했습니다. 따라서 nonzero byte `+0xfe`가 handle-retention 분기를 선택해 다음 descriptor 경계를 여는 데 인과적입니다.

**미확정 — 실제 응답 의미.** `0x0001`은 원본 소비 분기의 인과성을 검증하기 위한 추정값이며 실제 Hardlock driver나 dongle에서 관찰한 응답이 아닙니다. 과거 stale runtime 로그도 request-side buffer 보존만 보여 물리 응답 근거가 되지 않습니다. 이 결과는 `0x458` 호출 도달 조건만 복원하며, 각 264바이트 Function `0x0e` 호출의 유효 마지막 8바이트 출력과 실제 세 seed는 계속 미확정입니다.

*Confirmed — current tail and consumer branch, 2026-09-01. Run `20260901-003431-926` tied an exact 256-byte descriptor marker to a `0x44c`-filtered post-IOCTL trace. The Function-0 request tail word at offset `0xfe` was `0x0000`. The IOCTL succeeded at return address `0x00a4ef96`, preserving the buffer and reporting 256 bytes. The upper helper checks success at `0x00a4ecd2`, loads descriptor byte `+0xfe` at `0x00a4ed17`, and tests it at `0x00a4ed1d`. The current zero path closes the handle, resets the global handle to `0xffffffff`, and returns status zero.*

*Confirmed — synthetic-tail causality. The default-off `--device-mock-hardlock-44c-tail` option writes a user-supplied 16-bit word only to the final two output bytes of an exact-size Function-0 `0x9c40244c`. Run `20260901-004347-276`, combining the Task 109 replay with synthetic `tail=0x0001`, recorded the first Function-0 request with `tail_word=0x0000`, then reached thirty Function-6 `0x44c` calls and thirty Function-`0x0e` `0x458` calls. A nonzero byte at `+0xfe` is therefore causal for selecting the handle-retention branch and opening the next descriptor boundary.*

*Unresolved — physical response semantics. Value `0x0001` is inferred solely to test causality in the original consumer branch; it was not observed from a physical Hardlock driver or dongle. Historical stale-runtime logs only preserved request-side buffers and provide no physical-response evidence. This result reconstructs only the condition for reaching `0x458`; the valid final eight output bytes for each 264-byte Function-`0x0e` call and the physical three seeds remain unresolved.*
