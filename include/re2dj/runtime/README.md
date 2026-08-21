# include/re2dj/runtime

게스트 주소 공간, 레지스터 컨텍스트, 실행 backend 인터페이스의 **공개 헤더**가 들어갈 자리입니다.

*Public headers for the guest address space, register context, and execution backend interface.*

`GuestAddress`(32비트 값 타입), `AddressSpace`, `GuestContext`, `ExecutionBackend`를 여기에 둡니다. 호스트 포인터를 노출하지 않으며 호스트 OS 헤더를 포함하지 않습니다.

*`GuestAddress` (a 32-bit value type), `AddressSpace`, `GuestContext`, and `ExecutionBackend` belong here. Nothing exposes host pointers or includes host OS headers.*
