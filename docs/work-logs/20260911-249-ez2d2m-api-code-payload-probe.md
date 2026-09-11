# 작업 로그: ez2d2m API_CODE payload의 블록별 역할 확인

## 한국어

### 관련 문서

- 분석: [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)의 `API_CODE` 7블록 payload 절
- 선행: [Hardlock 요청 단위 응답 행](20260911-247-hardlock-payload-response-rows.md)
- 지식: [Hardlock API function 코드](../kb/hardlock-api-functions.md)

### 요구사항

`ez2d2m` 분석을 잇습니다. 작업 247이 남긴 유일한 저장소 내부 항목은 아직 변화시켜 보지 않은 블록 3과 블록 6이었습니다.

코드는 바꾸지 않았습니다. 기존 요청 행 기능으로 수행한 관측입니다.

### 방법

요청 행의 입력 패턴은 실행 간 안정적인 블록 3·5·6만 지정하고 나머지는 `??`로 두었습니다. 출력만 다른 map을 만들어 각각 두 번 실행하고, 줄 수 대신 Hardlock 요청 구성·port 접근 수·프로세스 종료 코드로 비교했습니다. 줄 수는 실행마다 흔들려 지표로 쓸 수 없었습니다.

대조군은 블록 5를 같은 값으로 되쓰는 항등 행입니다. 출력이 전부 `??`인 행은 파서가 "request row must specify at least one byte"로 거절하므로 쓸 수 없습니다.

### 결과 — 확인됨

| map | 출력 변경 | Hardlock | io-port | 종료 |
| --- | --- | --- | --- | --- |
| A 대조군 | 블록 5 항등 | total 32, descriptor 15, transform 12 | 8 | `0` |
| B3 | 블록 3 마지막 바이트 +1 | **A와 동일** | 8 | `0` |
| B6a | 블록 6 **첫** dword 변경 | A와 동일 | 8 | `0` |
| B6b | 블록 6 **두 번째** dword 변경 | total 31, descriptor 14 | **0** | **`0xC0000005`** |

**블록 6의 두 번째 dword는 게스트 주소입니다.** 블록 6 `001d6b5f00db1900`은 little-endian dword 둘로 `0x5f6b1d00`(injected runtime 대역)과 `0x0019db00`(게스트 스택 대역)입니다. 뒤를 바꾸면 게스트가 램프 순서열에 도달하지도 못하고 치명적 access violation으로 죽습니다. 앞을 바꾸면 대조군과 구분되지 않습니다.

따라서 블록 6은 응답의 일부가 아니라 게스트가 넘긴 포인터이고, 응답 행은 이 블록을 그대로 되돌려 주어야 합니다. 2EZConfig-V2 계약 요약의 "마지막 블록은 유지"와 맞습니다.

### 결론하지 않은 것

**B3가 대조군과 같다는 것을 "게스트가 블록 3을 무시한다"로 읽지 않았습니다.** echo와 echo+1은 둘 다 틀린 답일 수 있고, 틀린 답끼리 구분되지 않는 것은 당연합니다. 작업 247의 "echo와 `HL_CODE` 답이 블록 0·1·2·4·5에서 달라도 downstream 동일" 관측도 같은 한계를 갖습니다.

지금 말할 수 있는 것은 **두 틀린 답 사이에 관측 가능한 차이가 없다**까지입니다. 이 구분은 이 작업에서 의식적으로 지켰습니다. 작업 245에서 "순서상 앞"과 "원인"을 섞어 틀린 결론을 낸 적이 있기 때문입니다.

### 추정

블록 6의 두 번째 dword가 게스트 스택을 가리키고 payload에 `cc` 채움(초기화되지 않은 스택)이 섞여 있다는 점을 함께 보면, 이 7블록은 게스트가 스택에 잡은 구조체이고 그 포인터가 동글이 답을 쓸 버퍼일 수 있습니다. descriptor 요청의 `data_address`와 같은 모양입니다.

맞다면 payload 블록을 어떻게 채워도 게스트가 보는 값이 바뀌지 않으므로 블록 0–5 변경이 관측 차이를 내지 않는 것도 설명됩니다. 포인터를 망가뜨리면 죽는다는 사실은 게스트가 그 주소를 쓴다는 뜻일 뿐, 읽는지 쓰는지는 구분하지 않았습니다.

### 검증

| 항목 | 결과 |
| --- | --- |
| 각 map 2회 실행 | map마다 두 실행의 지표가 같음 |
| 코드 변경 | 없음. `git diff -- src/ include/ tests/` 비어 있음 |
| 시험 map과 입력 dump | Git이 무시하는 `cfg/ez2d2m/`에만 존재 |

### 남은 것

- **유효한 `API_CODE` 답.** 외부 도구의 몫이며, 블록 6은 그대로 되돌려 주어야 한다는 제약이 추가됐습니다.
- 동글이 블록 6 포인터가 가리키는 버퍼에 쓰는지. 확인되면 guest-memory writer가 필요하고, 그때도 관측된 크기와 내용만 반영해야 합니다.
- 게스트가 그 주소를 읽는지 쓰는지 구분. 현재 진단에는 읽기 감시가 없습니다.

## English

### Related documents

- Analysis: the `API_CODE` seven-block payload section of [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md)
- Preceding: [Hardlock request-level response rows](20260911-247-hardlock-payload-response-rows.md)
- Knowledge base: [Hardlock API function codes](../kb/hardlock-api-functions.md)

### Requirement

Continue the `ez2d2m` analysis. The only in-repository item task 247 left was blocks 3 and 6, which had never been varied. No code changed; this is observation through the existing request-row feature.

### Method

The request row's input pattern named only blocks 3, 5 and 6 — the ones stable between runs — leaving the rest `??`. Maps differing only in output were each run twice and compared by Hardlock request composition, port-access count and process exit code rather than trace length, which varies per run and is unusable as a metric.

The control is an identity row writing block 5 back unchanged; a row whose output is entirely `??` is rejected by the parser as specifying no byte.

### Results — confirmed

The control gives 32 Hardlock requests, 15 descriptors, 12 transforms, eight port writes and exit `0`. Changing block 3's last byte is identical to it. Changing block 6's first dword is also identical. Changing block 6's **second** dword gives 31 requests, 14 descriptors, **no port writes** and a fatal **`0xC0000005`**.

**Block 6's second dword is a guest address.** The block is `001d6b5f00db1900`, two little-endian dwords `0x5f6b1d00` in the injected runtime's load region and `0x0019db00` in the guest stack range. Corrupting the second kills the guest before it reaches the lamp sequence; corrupting the first is indistinguishable from the control.

Block 6 is therefore a pointer the guest supplied rather than part of the answer, and a response row must return it unchanged — agreeing with the 2EZConfig-V2 contract summary's "the last block is left unchanged".

### What was deliberately not concluded

B3 matching the control was **not** read as "the guest ignores block 3". Echo and echo-plus-one may both be wrong, and two wrong answers being indistinguishable proves nothing; task 247's "echo and `HL_CODE` differ across blocks 0, 1, 2, 4 and 5 with identical downstream" carries the same limit.

**No observable difference between two wrong answers** is the whole claim. The distinction was kept deliberately, because task 245 produced a wrong conclusion by conflating "precedes" with "causes".

### Inferred

With the payload's `cc` filler being uninitialised stack and the second dword pointing into the guest stack, these seven blocks look like a stack structure whose pointer names the buffer the dongle writes its answer into — the same shape as a descriptor's `data_address`. If so, nothing written into the payload changes what the guest reads, which would explain the absence of any observable difference across blocks 0 to 5. That corrupting the pointer is fatal shows only that the guest uses the address; read versus write was not distinguished.

### Verification

Each map was run twice with matching figures. No code changed — `git diff -- src/ include/ tests/` is empty — and the test maps and input dumps exist only under the Git-ignored `cfg/ez2d2m/`.

### What remains

A valid `API_CODE` answer, still the external tool's part, now with the added constraint that block 6 must be returned unchanged. Whether the dongle writes into the buffer that pointer names, which would require a guest-memory writer reflecting only observed size and content. And distinguishing a read from a write at that address, for which the current diagnostics have no read watch.
