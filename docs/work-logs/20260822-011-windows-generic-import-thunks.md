# 작업 로그: Windows import별 native thunk

## 결과

Win32 x86 helper의 단일 `probe.dll!ProbeGate` parser를 일반 PE32 import descriptor/thunk 순회로 교체했습니다. 이름 import와 ordinal import를 모두 `ImportGateTable`에 바인딩하며, 중복 조합은 같은 synthetic gate와 native thunk를 재사용합니다. parser와 machine-code emitter는 `native_import_thunks.h/.cpp`로 분리하고 helper 진입점에는 image mapping, protocol과 실행 orchestration을 남겼습니다.

## Result

Replaced the Win32 x86 helper's single `probe.dll!ProbeGate` parser with general PE32 import-descriptor/thunk traversal. Both named and ordinal imports bind through `ImportGateTable`, while duplicate combinations reuse one synthetic gate and native thunk. The parser and machine-code emitter live in `native_import_thunks.h/.cpp`, leaving image mapping, protocol, and execution orchestration in the helper entry point.

## Protocol v2와 metadata

protocol version을 2로 올리고 `LoadResult.import_count`와 variable-size `ImportMetadata` packet을 추가했습니다. helper가 module, name 또는 ordinal, synthetic gate 주소를 보내면 x64 adapter가 크기·형태·중복 gate를 검증하고 `LoadedPeImage.imports`를 채웁니다. 실행 event의 `gate_address`도 native 함수 주소가 아닌 같은 synthetic gate identity입니다.

## Protocol v2 and metadata

Bumped the protocol to version 2 and added `LoadResult.import_count` plus variable-size `ImportMetadata` packets. The helper sends module, name or ordinal, and synthetic gate address; the x64 adapter validates size, shape, and duplicate gates before populating `LoadedPeImage.imports`. Execution-event `gate_address` now carries the same synthetic identity rather than a native function address.

## Native thunk와 호출 규약

각 고유 import에 19바이트 x86 thunk를 생성해 IAT에 실제 실행 주소를 씁니다. thunk는 synthetic gate를 공용 bridge에 전달하고, bridge는 guest return slot을 기준으로 event를 보냅니다. completion 뒤 64비트 반환으로 EDX:EAX를 복원하고 host가 지정한 byte 수만큼 ESP를 정리한 뒤 guest return address로 이동합니다. 현재 protocol은 event 하나를 직렬 처리하므로 cleanup slot도 하나이며 멀티스레드 확장 때 event별 resume frame이 필요합니다.

## Native thunk and calling convention

Each unique import gets a 19-byte x86 thunk whose executable address is written into the IAT. The thunk passes its synthetic gate to the shared bridge, which reports an event relative to the guest return slot. After completion, a 64-bit return restores EDX:EAX, the thunk removes the host-specified number of bytes from ESP, and control jumps to the guest return address. The protocol still serializes one event and therefore has one cleanup slot; multithreading will require per-event resume frames.

## Synthetic 검증

synthetic PE32가 `probe.dll!ProbeGate`와 ordinal `#7`을 연속 호출하도록 확장했습니다. 첫 event에서 인자 41에 EAX 42를 반환하고, guest가 이를 둘째 인자로 전달합니다. 둘째 event에는 EDX:EAX `1:43`을 반환하며 guest가 두 register를 더해 44로 종료합니다. 이 과정에서 metadata 순서, gate 주소, event ID, memory read/write, stack cleanup, EDX:EAX와 child exit code를 확인했습니다.

## Synthetic verification

Extended the synthetic PE32 to call `probe.dll!ProbeGate` and ordinal `#7` in sequence. The first event returns EAX 42 for argument 41, which the guest passes as the second argument. The second returns EDX:EAX `1:43`; guest code adds both registers and exits with 44. This verifies metadata order, gate addresses, event IDs, memory read/write, stack cleanup, EDX:EAX, and child exit code.

```text
native-ipc-host-probe: load=0x10000000 entry=0x10001000 imports=2 arguments=41,42 result=44 child=0
```

## 검증

* Windows x64 warnings-as-errors 전체 build — 성공
* Windows x64 unit CTest — 1/1 통과
* 기존 Win32 native gate probe — 1/1 통과
* protocol v2 adapter integration script — 성공

## Verification

The complete Windows x64 warnings-as-errors build passed, the x64 unit CTest passed 1/1, the existing Win32 native-gate probe passed 1/1, and the protocol-v2 adapter integration script succeeded.
