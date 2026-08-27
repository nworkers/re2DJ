# src/runtime

런타임 코어 구현을 둡니다. 현재 게스트 주소 공간과 PE32 이미지 로더가 구현되어 있습니다.

*Runtime-core implementations. The guest address space and PE32 image loader are implemented.*

`address_space.cpp`는 4 KiB 단위 게스트 매핑과 little-endian 접근자를, `pe_loader.cpp`는 섹션 매핑·재배치·import gate 바인딩을 담당합니다. 공개 `ExecutionBackend` event/reply 및 게스트 메모리 접근 경계는 `include/re2dj/runtime/execution_backend.h`에 있습니다. Windows와 Linux의 `NativeHelperBackend`가 각각 별도 32비트 x86 helper를 이 경계 뒤에 두며, 직접 인터프리터는 후순위입니다. Linux 제품 CLI 연결과 향후 공용 HLE 구조는 [ARCHITECTURE.md](../../ARCHITECTURE.md) 7절에 둡니다.

*`address_space.cpp` provides 4 KiB guest mappings and little-endian accessors; `pe_loader.cpp` maps sections, applies relocations, and binds import gates. The public `ExecutionBackend` event/reply and guest-memory boundary lives in `include/re2dj/runtime/execution_backend.h`. The Windows and Linux `NativeHelperBackend` implementations each place a separate 32-bit x86 helper behind this boundary; a custom interpreter is deferred. Section 7 of [ARCHITECTURE.md](../../ARCHITECTURE.md) covers the Linux product-CLI connection and planned shared HLE structure.*
