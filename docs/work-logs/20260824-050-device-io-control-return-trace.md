# LPTDI DeviceIoControl 반환 추적 작업 로그

관련 설계: [LPTDI DeviceIoControl 반환 추적](../design/20260824-050-device-io-control-return-trace.md)  
관련 작업 지시: [작업 지시 050](../work-orders/20260824-050-device-io-control-return-trace.md)

## 결과

canonical mock-on의 두 LPTDI IOCTL에 대해 entry 8개 인자와 return 결과를 두 번씩 완결해 기록했다. 두 호출은 모두 host에서 FALSE를 반환했고 input, output, bytes-returned를 수정하지 않았다. 이후 guest는 기존과 동일한 손상 `.data` initializer AV에 도달했다.

## 관찰 결과

| 항목 | IOCTL `0x9c406410` | IOCTL `0x9c406414` |
| --- | --- | --- |
| caller | `0x01ed4240` | `0x01ed4dba` |
| handle | `0xFEED0001` | `0xFEED0001` |
| input | 4 bytes, 실행별 변화 | 24 bytes, 일부 필드 실행별 변화 |
| output | 8 bytes | 104 bytes |
| overlapped | null | null |
| EAX | 0 / FALSE | 0 / FALSE |
| input 변화 | 없음 | 없음 |
| output 변화 | 없음 | 없음 |
| bytes-returned 변화 | 0 → 0 | `0x01ed49d9` → 동일 |

첫 실행 input:

* `0x9c406410`: `a60db811`
* `0x9c406414`: `8c62bb2f00000000000000000cd727f6dd9c7bd400000000`

두 번째 실행 input:

* `0x9c406410`: `ff3c3c74`
* `0x9c406414`: `332e576700000000000000008c5f2f5e5d14737c00000000`

output의 호출 전 내용은 두 실행에서 같았고 반환 뒤에도 그대로였다. 두 번째 bytes-returned의 `0x01ed49d9`는 보호 스텁의 앞선 GetVersion caller 주소와 일치하는 stack 잔존값이므로 API가 값을 쓰지 않았다는 추가 증거다.

CTL_CODE bitfield 해석은 두 요청이 vendor device type `0x9c40`, read access, `METHOD_BUFFERED`, function `0x904`·`0x905`임을 보여준다. 입력이 실행마다 달라지는 점은 두 단계 challenge-response protocol 가능성을 지지한다.

## 구현

API watch metadata에 argument count를 추가했다. guest main image에서 직접 들어온 DeviceIoControl만 8개 인자와 최대 64바이트 buffer를 기록한다. thread별 one-shot breakpoint를 guest return address에 설치해 EAX, input/output post-snapshot, bytes-returned를 기록하고 원래 byte/EIP를 복원한다. kernel32에서 KernelBase로 forwarding되는 내부 호출은 일반 API call로만 기록해 return trace 중복을 피한다.

진단은 API 반환값이나 guest buffer를 수정하지 않았다. return breakpoint가 기존 흐름을 보존했는지는 두 실행 모두 동일한 `0x19d521bd` AV와 `.data` window에 도달한 것으로 확인했다.

## 검증

Windows x86 Debug build와 CTest 2/2가 통과했다. 비교 로그:

* `20260824-010042-488.jsonl`
* `20260824-010117-346.jsonl`

## 해석 경계와 다음 단계

확인됨: 두 host IOCTL은 실패하며 output이나 bytes-returned를 제공하지 않는다. 입력은 실행별 challenge를 포함한다. 실패 뒤 `.data` initializer는 동일하게 잘못 복원된다.

추정: protection이 output을 복원 key/material로 필요로 할 가능성이 높다. 단, BOOL 성공만으로 다른 분기를 선택할 수도 있으므로 아직 output data 필요성을 확정하지 않는다.

다음 단계는 DeviceIoControl HLE가 두 요청에 `TRUE`, bytes-returned 0, buffer 무변화를 반환하는 최소 perturbation이다. 이것으로 BOOL 성공만 필요한지, 실제 challenge response가 필요한지 분리한다.

---

# LPTDI DeviceIoControl Return Trace Work Log

Related design: [LPTDI DeviceIoControl Return Trace](../design/20260824-050-device-io-control-return-trace.md)  
Related work order: [Work Order 050](../work-orders/20260824-050-device-io-control-return-trace.md)

## Result

Captured complete eight-argument entry and return records twice for both LPTDI IOCTLs on the canonical mock-on path. Both host calls returned FALSE without changing input, output, or bytes-returned, after which the guest reached the same corrupt `.data` initializer AV.

IOCTL 0x9c406410 uses caller 0x01ed4240, a four-byte run-varying input, and an eight-byte output. IOCTL 0x9c406414 uses caller 0x01ed4dba, a 24-byte partially varying input, and a 104-byte output. Both are synchronous. The second bytes-returned value remains stack-residual stub address 0x01ed49d9. CTL_CODE decoding yields vendor device type 0x9c40, read access, METHOD_BUFFERED, and functions 0x904/0x905, supporting a two-stage challenge-response protocol.

## Implementation and verification

API watch metadata now carries argument count. Guest-origin DeviceIoControl calls record all eight arguments and bounded 64-byte buffers. A per-thread one-shot breakpoint at the guest return address records EAX, post-call buffers, and bytes-returned, then restores the original byte/EIP. Internal kernel32-to-KernelBase forwarding is not duplicated as a return trace. No API result or guest buffer is modified.

Windows x86 Debug build and CTest 2/2 passed. Evidence logs are `20260824-010042-488.jsonl` and `20260824-010117-346.jsonl`.

## Interpretation boundary and next step

Confirmed: both host IOCTLs fail without output, their inputs contain run-varying challenges, and the same malformed initializer follows. It is likely but not confirmed that output carries restoration key/material; success BOOL alone may select a different path.

The next experiment returns TRUE with zero bytes and unchanged buffers for both requests. This separates BOOL success from required challenge-response data.
