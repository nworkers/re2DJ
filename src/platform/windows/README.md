# src/platform/windows

Windows 전용 backend와 probe를 둡니다. `native_helper_backend.cpp`는 x86 helper process와 protocol v3 IPC를 `ExecutionBackend`로 캡슐화하고 import metadata를 수신합니다. `native_pe_image.cpp`는 requested-base mapping, relocation, protection과 TLS callback을, `native_import_thunks.cpp`는 이름/ordinal import 해석과 native thunk 생성을 담당합니다. `native_ipc_helper.cpp`는 protocol 및 실행을 조율합니다. 두 probe는 최소 gate 호출과 non-preferred synthetic PE32 왕복을 검증합니다.

*Windows-specific backends and probes. `native_helper_backend.cpp` encapsulates the x86 helper process and protocol-v3 IPC as an `ExecutionBackend` and receives import metadata. `native_pe_image.cpp` owns requested-base mapping, relocations, protection, and TLS callbacks; `native_import_thunks.cpp` parses named/ordinal imports and emits native thunks. `native_ipc_helper.cpp` orchestrates protocol and execution. The two probes validate a minimal gate call and a non-preferred synthetic-PE32 round trip.*

rePIU와 달리 주 host는 64비트이므로 디렉터리 이름에 비트 폭을 넣지 않습니다. 32비트 helper도 Windows platform 구현이므로 이 아래에 둡니다. 여기서만 `<windows.h>`를 포함할 수 있습니다.

*Unlike rePIU the primary host is 64-bit, so the directory name carries no bit width. The 32-bit helper also lives here as a Windows platform implementation. This is the only place that may include `<windows.h>`.*
