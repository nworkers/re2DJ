# 작업 로그: Linux native x86 helper 최소 prototype

## 결과

Linux x86-64 host가 별도 i386 helper를 `fork`/`exec`하고 두 anonymous pipe로 protocol v3 event/reply를 처리하는 최소 prototype을 구현했습니다. helper의 컴파일된 x86 `__stdcall NativeImportGate(41)`이 return address, stack pointer와 synthetic gate `0xF0000000`을 event로 보냅니다. host는 `ESP + 4`의 인자 41을 읽고 같은 값을 다시 쓴 뒤 EAX 42와 stack cleanup 4를 응답합니다.

## Result

Implemented a minimal prototype in which a Linux x86-64 host launches a separate i386 helper through `fork`/`exec` and handles protocol-v3 event/reply over two anonymous pipes. The helper's compiled x86 `__stdcall NativeImportGate(41)` sends its return address, stack pointer, and synthetic gate `0xF0000000` as an event. The host reads argument 41 at `ESP + 4`, writes it back, and replies with EAX 42 plus four bytes of stack cleanup.

## 공용 protocol

OS type을 포함하지 않던 Windows protocol v3 header를 `src/platform/native_helper_protocol.h`로 이동하고 namespace를 `re2dj::platform::native_protocol`로 공용화했습니다. Windows backend/helper와 Linux probe가 같은 packet layout과 magic/version 검증을 사용합니다.

## Shared protocol

Moved the OS-independent Windows protocol-v3 header to `src/platform/native_helper_protocol.h` and generalized its namespace to `re2dj::platform::native_protocol`. Windows backend/helper and Linux probes now use the same packet layout and magic/version validation.

## Build와 환경

WSL Ubuntu 24.04에 검증 dependency로 `g++-multilib`와 `ninja-build`를 설치했습니다. 저장소에는 third-party code를 추가하지 않았습니다. `linux-x86-native-probe` preset은 `-m32`와 frame pointer 보존을 사용해 helper를 별도 ELF32로 빌드합니다. 반복 가능한 `scripts/test_linux_native_helper_probe.sh`는 x64 전체 build/CTest, i386 helper build, architecture 출력과 integration 실행을 수행합니다.

## Build and environment

Installed `g++-multilib` and `ninja-build` as verification dependencies in WSL Ubuntu 24.04. No third-party code was added to the repository. The `linux-x86-native-probe` preset uses `-m32` and preserves frame pointers to build the helper as a separate ELF32 binary. Repeatable `scripts/test_linux_native_helper_probe.sh` runs the complete x64 build/CTest, i386 helper build, architecture reporting, and integration.

## 안전 범위와 제한

helper는 gate event가 pending인 동안 return slot과 첫 인자를 포함하는 8바이트 stack 범위만 memory read/write 대상으로 허용하며 64비트 산술로 범위를 검증합니다. 현재는 컴파일된 gate feasibility만 확인했습니다. Linux PE32 mapping, 범용 import thunk, `ExecutionBackend` adapter와 병렬 thread는 후속 작업입니다.

## Safety range and limitations

While a gate event is pending, the helper permits memory reads/writes only in the eight-byte stack range containing the return slot and first argument, with bounds checked using 64-bit arithmetic. This validates compiled-gate feasibility only. Linux PE32 mapping, generic import thunks, an `ExecutionBackend` adapter, and parallel threads remain follow-up work.

## 검증

* WSL Ubuntu 24.04 Linux x86-64 warnings-as-errors 전체 build — 성공
* Linux x86-64 unit CTest — 1/1 통과
* Linux helper architecture — ELF 32-bit Intel 80386 확인
* Linux host architecture — ELF 64-bit x86-64 확인
* Linux x64/i386 native gate integration — 성공
* Windows x64 warnings-as-errors 전체 build 및 unit CTest — 성공, 1/1 통과
* 기존 Windows Win32 gate probe 및 protocol v3 IPC integration — 성공

통합 출력:

```text
linux-native-helper-probe: host=x86_64 helper=i386 argument=41 result=42 child=0
```

## Verification

The WSL Ubuntu 24.04 Linux x86-64 warnings-as-errors build and unit CTest passed, the helper and host were confirmed as ELF32 i386 and ELF64 x86-64 respectively, and native-gate integration succeeded with the output above. The Windows x64 warnings-as-errors build/unit suite, existing Win32 gate probe, and protocol-v3 IPC integration also remained green.
