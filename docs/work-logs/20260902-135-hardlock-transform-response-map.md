# Hardlock transform 응답 매핑 작업 로그

관련 설계: [Hardlock transform 응답 매핑](../design/20260902-135-hardlock-transform-response-map.md)
관련 작업 지시: [Hardlock transform 응답 매핑](../work-orders/20260902-135-hardlock-transform-response-map.md)

*Related design and work order: [Hardlock transform response map](../design/20260902-135-hardlock-transform-response-map.md), [work order](../work-orders/20260902-135-hardlock-transform-response-map.md).*

## 결과

- 플랫폼 중립 `hardlock_transform_responses` parser를 추가했습니다. 한 줄에 입력 16 hex와 출력 16 hex를 두고 `#` 주석과 빈 줄을 허용하며, 중복 입력과 형식 오류를 거절합니다.
- `HardlockStubDevice`에 응답 매핑 옵션을 추가했습니다. `0x458`의 각 8바이트 block을 조회해 맞으면 출력에 쓰고, 없으면 기존 항등 통과를 유지합니다. 결과에 `mapped`/`unmapped` 개수를 남깁니다.
- injected runtime에 매핑 buffer와 개수 export를 추가하고 스텁에 연결했습니다. re2DJ는 매핑을 계산하지 않고 받은 값을 적용만 합니다.
- 명시적 진단 옵션 `--hardlock-transform-inputs`와 launcher 옵션 `--hardlock-transform-map <path>`를 추가했습니다.
- 4th의 36개 challenge block을 수집하고 그 출처를 확정했습니다.

*Added the platform-neutral `hardlock_transform_responses` parser accepting one line per entry with sixteen hex digits of input and output, allowing `#` comments and blank lines while rejecting duplicate inputs and malformed lines. Added the response-map option to `HardlockStubDevice`, which looks up each eight-byte `0x458` block and writes the mapped output when found, keeping the identity passthrough otherwise and recording `mapped` and `unmapped` counts. Added the map buffer and count exports to the injected runtime and wired them to the stub; re2DJ never computes the mapping and only applies what it is given. Added the explicit `--hardlock-transform-inputs` diagnostic and the `--hardlock-transform-map <path>` launcher option, then collected 4th's 36 challenge blocks and established where they come from.*

## Function 0x0e challenge의 출처

두 번의 독립 실행이 동일한 36개 block을 같은 순서로 기록했습니다. 고유값은 32개이며 4개가 중복입니다.

각 block을 원본 이미지에서 조회한 결과, **모든 block이 32 KiB 청크 시작 8바이트와 정확히 일치**합니다. 청크는 section별로 raw 시작에서 `0x8000` 간격으로 걷습니다.

| section | raw 시작 | raw 크기 | 청크 수 |
| --- | --- | --- | --- |
| `.text` | `0x001000` | `0xdc000` | 28 |
| `.rdata` | `0x0dd000` | `0x00d000` | 2 |
| `.data` | `0x0ea000` | `0x01c000` | 4 |
| `.reloc` | `0x108000` | `0x00d000` | 2 |
| 합계 | | | **36** |

`.idata`(`0x106000`, `0x2000`)와 `.protect`(`0x115000`)에는 challenge가 없습니다.

**확인됨.** 보호 대상은 `.text` 하나가 아니라 `.idata`와 `.protect`를 제외한 이미지 전체입니다. challenge는 파일에서 결정되는 값이므로 실행마다 동일하고, 응답 매핑을 오프라인에서 한 번 만들어 재사용할 수 있습니다.

**추정.** `.idata`가 제외된 것은 Windows loader가 import table을 평문으로 읽어야 하기 때문으로 보입니다. 이 귀속은 별도로 확인하지 않았습니다.

*Two independent runs recorded the same 36 blocks in the same order, with 32 unique values and four repeats. Looking each block up in the original image shows **every block matches the first eight bytes of a 32 KiB chunk**, walked per section from each section's raw start at `0x8000` intervals: 28 chunks in `.text`, 2 in `.rdata`, 4 in `.data`, and 2 in `.reloc`, totalling exactly 36. `.idata` and `.protect` carry no challenge. **Confirmed:** the protected region is not `.text` alone but the whole image except `.idata` and `.protect`; because challenges are file-determined they are identical across runs, so a response map can be built offline once and reused. **Inferred:** `.idata` is likely excluded because the Windows loader must read the import table as plaintext, an attribution not separately verified.*

## 주입 경로 등가 검증

수집한 32개 고유 challenge에 대해 출력을 입력의 XOR `0xff`로 계산한 합성 매핑을 만들어 주입했습니다. 이 매핑은 기존 `--hardlock-transform-xor ff` probe와 동일한 출력을 내야 합니다.

- 36개 transform 요청 전부 `mapped=1:unmapped=0`으로 처리되었습니다.
- fault 시점 레지스터가 기존 XOR probe 실행과 일치합니다. `eax=0x00000000`, `ecx=edx=esi=edi=0x00ae0240`, `ebp=0x75295d49`, `esp=0x001beb34`, `flags=0x00010246`.
- `eip`와 `ebx`만 다릅니다. XOR probe 실행은 `0x0024d000`, 매핑 실행은 `0x00313000`이며 둘 다 이미지 밖 주소입니다.

**확인됨.** 매핑 주입 경로는 XOR probe와 동작상 등가입니다. 외부에서 계산한 응답을 원본 실행에 전달하는 경계가 작동합니다.

**확인됨 — 판별 기준 주의.** fault 주소는 실행마다 달라지는 할당 주소입니다. 따라서 후보 판별에 fault 주소를 그대로 쓰면 안 됩니다. 이미지 안인지 밖인지 같은 질적 구분이나 복호화된 영역의 통계를 써야 합니다.

*Injection-path equivalence: a synthetic map computing each output as its input XOR `0xff` was built from the 32 unique challenges, which must match the existing `--hardlock-transform-xor ff` probe. All 36 transform requests were handled with `mapped=1:unmapped=0`, and the fault-time registers match the earlier probe run — `eax=0x00000000`, `ecx=edx=esi=edi=0x00ae0240`, `ebp=0x75295d49`, `esp=0x001beb34`, `flags=0x00010246` — with only `eip` and `ebx` differing (`0x0024d000` versus `0x00313000`), both outside any image. **Confirmed:** map injection is behaviorally equivalent to the probe, so the boundary that carries externally computed responses into the original execution works. **Confirmed — a caution for judging candidates:** the fault address is a per-run allocation address, so it must not be used directly as a discriminator; use a qualitative distinction such as inside-image versus outside-image, or statistics of the decrypted region.*

## 검증

- Windows x86 Debug build 통과
- Unit tests: `1167` checks, `0` failures
- 선택 CTest: `3/3` 통과
- 4th 입력 목록이 두 실행에서 동일
- 36개 challenge 전부가 이미지의 32 KiB 청크 시작과 일치
- 합성 매핑 주입이 XOR probe와 등가
- 매핑 파일은 `/cfg/` ignore 규칙으로 Git 추적 대상에 나타나지 않음
- `git diff --check` 통과

*Verification: the Windows x86 Debug build passes, unit tests report 1,167 checks with zero failures, selected CTest passes 3/3, the 4th input list is identical across two runs, all 36 challenges match 32 KiB chunk starts in the image, synthetic map injection is equivalent to the XOR probe, map files stay untracked under the `/cfg/` ignore rule, and `git diff --check` passes.*

## 다음 단계

1. 복호화된 영역의 엔트로피와 x86 prologue 빈도로 후보를 자동 판별하는 도구를 만듭니다. fault 주소는 판별에 쓰지 않습니다.
2. 응답을 계산하는 별도 프로젝트에서 seed 후보를 얻으면 매핑 파일로 변환해 주입합니다. 이 저장소는 변환 알고리즘을 구현하지 않습니다.

*Next: build a judge scoring candidates from the decrypted region's entropy and x86 prologue frequency, never from the fault address; and once the separate response-computing project yields seed candidates, convert them into map files for injection, with this repository still implementing no transform algorithm.*
