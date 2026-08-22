# 작업 로그: Windows native helper relocation과 TLS callback

## 결과

Windows native helper protocol을 v3로 올리고 `LoadImageRequest`에 requested base와 PE file size를 추가했습니다. `NativeHelperBackend`는 requested base가 0이면 preferred base를, 아니면 지정된 32비트 주소를 helper에 전달하며 실제 `LoadResult` base와 entry point를 검증합니다.

## Result

Bumped the Windows native-helper protocol to v3 and added requested base plus PE-file size to `LoadImageRequest`. `NativeHelperBackend` sends the preferred base when the request is zero or the explicit 32-bit address otherwise, then validates the actual `LoadResult` base and entry point.

## Native PE image

helper의 image mapping 책임을 `native_pe_image.h/.cpp`로 추출했습니다. 이 하위 시스템은 exact-address `VirtualAlloc`, header/section copy와 zero-fill, image/entry 범위 검증, `ABSOLUTE`/`HIGHLOW` relocation, import thunk 연결, section protection, TLS callback 실행과 자원 해제를 소유합니다. protocol과 import gate bridge는 `native_ipc_helper.cpp`에 남겼습니다.

## Native PE image

Extracted helper image mapping into `native_pe_image.h/.cpp`. This subsystem owns exact-address `VirtualAlloc`, header/section copying and zero-fill, image/entry range validation, `ABSOLUTE`/`HIGHLOW` relocation, import-thunk binding, section protection, TLS callback execution, and resource release. Protocol and the import-gate bridge remain in `native_ipc_helper.cpp`.

## Relocation과 TLS

실제 load base와 preferred `ImageBase`의 delta가 있을 때 directory 5를 block 단위로 검증하고 모든 `HIGHLOW` target을 import binding 전에 보정합니다. `Start` 이후 PE32 TLS directory의 callback VA 배열을 실제 image 범위로 변환하고, 각 callback을 `(module_base, DLL_PROCESS_ATTACH, nullptr)`로 entry point 전에 호출합니다.

이번 구현은 process-attach callback 실행만 포함합니다. TLS raw template, loader TLS index, thread별 storage와 thread attach/detach callback은 멀티스레드 backend 작업에 남겼습니다.

## Relocation and TLS

When actual load base differs from preferred `ImageBase`, directory 5 is validated block by block and every `HIGHLOW` target is adjusted before import binding. After `Start`, the PE32 TLS callback VA array is converted against the actual image range, and each callback is invoked with `(module_base, DLL_PROCESS_ATTACH, nullptr)` before the entry point.

This implementation covers process-attach callback execution only. TLS raw templates, loader TLS indices, per-thread storage, and thread attach/detach callbacks remain for multithreaded backend work.

## Synthetic 검증

preferred base `0x10000000`인 4-section synthetic PE32를 requested base `0x11000000`에 적재했습니다. relocation은 두 import call operand, entry의 TLS state read, callback의 state write, TLS directory callback-array VA와 callback VA를 보정합니다. callback이 state 7을 기록하고 두 import가 만든 result 44에 entry가 이를 더해 51로 종료했습니다.

## Synthetic verification

Mapped a four-section synthetic PE32 with preferred base `0x10000000` at requested base `0x11000000`. Relocations adjust two import-call operands, the entry's TLS-state read, the callback's state write, the TLS directory callback-array VA, and callback VA. The callback records state 7; the entry adds it to the two-import result 44 and exits with 51.

```text
native-ipc-host-probe: load=0x11000000 entry=0x11001000 imports=2 arguments=41,42 result=51 child=0
```

## 검증

* Windows x64 warnings-as-errors 전체 build — 성공
* Windows x64 unit CTest — 1/1 통과
* 기존 Win32 native gate probe — 1/1 통과
* protocol v3 relocation/TLS integration script — 성공

## Verification

The complete Windows x64 warnings-as-errors build passed, the x64 unit CTest passed 1/1, the existing Win32 native-gate probe passed 1/1, and the protocol-v3 relocation/TLS integration script succeeded.
