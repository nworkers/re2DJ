# 작업 로그 342: 게스트 PE32 export facade builder / Work log 342: Guest PE32 export facade builder

설계: [게스트 PE32 export facade builder](../design/20260922-342-guest-pe-facade-builder.md)
작업 지시: [작업 지시 342](../work-orders/20260922-342-guest-pe-facade-builder.md)

*Design: [Guest PE32 export facade builder](../design/20260922-342-guest-pe-facade-builder.md)*
*Work order: [Work order 342](../work-orders/20260922-342-guest-pe-facade-builder.md)*

## 결과 / Result

`GuestModuleDescriptor`에서 합성 i386 PE32 DLL file image를 만드는 플랫폼 중립 `GuestPeFacadeBuilder`를 구현했습니다. 결과는 DOS/PE32 header, read-only `.edata`, execute/read `.text`, export directory/EAT/name pointer/ordinal table과 descriptor 순서의 19-byte bridge thunk를 포함합니다. explicit ordinal은 유지하고 ordinal이 없는 export에는 가장 낮은 unused ordinal을 deterministic하게 배정하며, sparse EAT와 ordinal-only export도 표현합니다.

*Implemented a platform-neutral `GuestPeFacadeBuilder` that creates a synthetic i386 PE32 DLL file image from a `GuestModuleDescriptor`. The result contains DOS/PE32 headers, read-only `.edata`, execute/read `.text`, the export directory/EAT/name-pointer/ordinal tables, and descriptor-ordered 19-byte bridge thunks. Explicit ordinals are preserved, exports without ordinals receive deterministic lowest-unused values, and sparse EAT plus ordinal-only exports are represented.*

기존 registry 내부 descriptor 검증을 `ValidateGuestModuleDescriptor`로 추출하여 registry와 builder가 이름/alias/export/ABI 유효성 계약을 공유하게 했습니다. embedded NUL도 공용 검증에서 거절합니다. builder는 image base·bridge·cleanup·export gate를 모두 `GuestAddress`로 받고 host pointer나 OS header를 사용하지 않습니다. checked 32-bit layout arithmetic 뒤에 local result를 완성한 경우에만 output을 교체하므로 실패는 transactional합니다.

*Extracted the former registry-local descriptor validation as `ValidateGuestModuleDescriptor`, giving the registry and builder one name/alias/export/ABI validity contract; embedded NULs are now rejected there as well. The builder receives image-base, bridge, cleanup, and export-gate values exclusively as `GuestAddress`, using neither host pointers nor OS headers. It replaces the caller's output only after completing checked 32-bit layout arithmetic into a local result, so failure is transactional.*

host memory mapping, preferred-base reservation, final R/RX protection, 실제 `kernel32` export descriptor와 registry 연결은 구현하지 않았습니다. 이 경계는 작업 343에서 Linux i386 mapping과 함께 연결합니다.

*Host-memory mapping, preferred-base reservation, final R/RX protection, the real `kernel32` export descriptor, and registry integration were not implemented. Task 343 connects those boundaries with Linux i386 mapping.*

## 검증 / Validation

- Windows x86 Debug warnings-as-errors: build 통과, CTest 1/1, 직접 실행 `checks: 2063, failures: 0`.
- Windows x64 Debug warnings-as-errors: build 통과, CTest 1/1, 직접 실행 `checks: 2063, failures: 0`.
- Linux x86 Debug warnings-as-errors: build 통과, CTest 1/1, 직접 실행 `checks: 2063, failures: 0`.
- Linux x64 Debug warnings-as-errors: build 통과, CTest 1/1, 직접 실행 `checks: 2063, failures: 0`.
- 독립 test parser가 PE32/i386/DLL header, section RVA/raw bounds, export module name, EAT gap, name 정렬, name-to-EAT ordinal index, ordinal-only export와 thunk opcode/gate/relative bridge/cleanup 주소를 확인했습니다.
- invalid descriptor/options, embedded NUL, duplicate ordinal, 잘못된 gate 수·0 주소·unaligned image base와 null output 거절을 확인했고 실패 후 기존 output 불변성을 확인했습니다.

*Validation performed:*

- *Windows x86 Debug warnings-as-errors: build passed, CTest 1/1, direct run `checks: 2063, failures: 0`.*
- *Windows x64 Debug warnings-as-errors: build passed, CTest 1/1, direct run `checks: 2063, failures: 0`.*
- *Linux x86 Debug warnings-as-errors: build passed, CTest 1/1, direct run `checks: 2063, failures: 0`.*
- *Linux x64 Debug warnings-as-errors: build passed, CTest 1/1, direct run `checks: 2063, failures: 0`.*
- *An independent test parser verifies PE32/i386/DLL headers, section RVA/raw bounds, export module name, EAT gaps, name ordering, name-to-EAT ordinal indices, ordinal-only exports, and thunk opcode/gate/relative-bridge/cleanup values.*
- *Tests cover rejection of invalid descriptors/options, embedded NULs, duplicate ordinals, wrong gate counts, zero addresses, unaligned image bases, and null output, including output invariance after failure.*
