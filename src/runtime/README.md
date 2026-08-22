# src/runtime

런타임 코어 구현을 둡니다. 현재 게스트 주소 공간과 PE32 이미지 로더가 구현되어 있습니다.

*Runtime-core implementations. The guest address space and PE32 image loader are implemented.*

`address_space.cpp`는 4 KiB 단위 게스트 매핑과 little-endian 접근자를, `pe_loader.cpp`는 섹션 매핑·재배치·import gate 바인딩을 담당합니다. 공개 `ExecutionBackend` event/reply 및 게스트 메모리 접근 경계는 `include/re2dj/runtime/execution_backend.h`에 있습니다. Windows `NativeHelperBackend`와 x86 helper가 requested-base relocation, TLS callback, 이름/ordinal import metadata 및 native thunk를 통해 이 경계를 구현하며, 직접 인터프리터는 후순위입니다. 설계는 [ARCHITECTURE.md](../../ARCHITECTURE.md) 7절입니다.

*`address_space.cpp` provides 4 KiB guest mappings and little-endian accessors; `pe_loader.cpp` maps sections, applies relocations, and binds import gates. The public `ExecutionBackend` event/reply and guest-memory boundary lives in `include/re2dj/runtime/execution_backend.h`. Windows `NativeHelperBackend` and the x86 helper implement it through requested-base relocation, TLS callbacks, named/ordinal import metadata, and native thunks; a custom interpreter is deferred. See section 7 of [ARCHITECTURE.md](../../ARCHITECTURE.md).*
