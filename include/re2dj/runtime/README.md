# include/re2dj/runtime

게스트 주소 공간, PE32 로더, 레지스터 컨텍스트, 실행 backend 인터페이스의 **공개 헤더**를 둡니다.

*Public headers for the guest address space, PE32 loader, register context, and execution backend interface.*

현재 `GuestAddress`, `AddressSpace`, `ImportGateTable`, `LoadPe32Image()`, `ExecutionBackend` event/reply와 guest memory read/write 경계가 구현되어 있습니다. 이후 `GuestContext`를 추가합니다. 호스트 포인터를 노출하지 않으며 호스트 OS 헤더를 포함하지 않습니다.

*`GuestAddress`, `AddressSpace`, `ImportGateTable`, `LoadPe32Image()`, and the `ExecutionBackend` event/reply and guest-memory read/write boundary are implemented. `GuestContext` follows later. Nothing exposes host pointers or includes host OS headers.*
