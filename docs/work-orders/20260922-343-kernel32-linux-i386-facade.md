# 작업 지시 343: kernel32 모듈과 Linux i386 facade mapping / Work order 343: kernel32 module and Linux i386 facade mapping

## 목표 / Objective

확인된 네 `kernel32` export descriptor를 추가하고 작업 342의 facade를 Linux i386에서 충돌 없이 mapping하여 기존 import bridge로 실행합니다.

*Add descriptors for the four confirmed `kernel32` exports and map the Task 342 facade without collision on Linux i386 for execution through the existing import bridge.*

## 구현 / Implementation

1. `kernel32_module.h/.cpp`에 `kernel32.dll`/`kernel32` identity, 네 export, stdcall argument metadata와 현재 진단 반환을 선언합니다.
2. Linux i386 `NativeGuestModuleSet`에 staged gate binding, facade build, exact-address 후보 mapping, RW→R/RX protection, registry 등록과 수명 관리를 구현합니다.
3. bridge event를 descriptor handler로 dispatch하고 EAX/EDX 및 stdcall cleanup을 전달합니다.
4. 공용 descriptor 단위 테스트와 Linux i386 실제 mapping/실행/protection probe를 추가합니다.

*Implementation: declare `kernel32.dll`/`kernel32` identity, four exports, stdcall argument metadata, and current diagnostic returns in `kernel32_module.h/.cpp`; implement staged gate binding, facade build, exact-address candidate mapping, RW-to-R/RX protection, registry registration, and lifetime ownership in a Linux i386 `NativeGuestModuleSet`; dispatch bridge events to descriptor handlers with EAX/EDX and stdcall cleanup; and add shared descriptor unit tests plus a Linux i386 real mapping/execution/protection probe.*

## 완료 기준 / Acceptance criteria

- descriptor와 facade export가 확인된 네 API에 대해 일치합니다.
- registry의 `kernel32` handle은 실제 facade base이고 모든 export 주소가 facade image 안에 있습니다.
- Linux i386에서 facade `GetVersion`과 `CreateFileA` thunk가 기존 bridge를 왕복하며 각각 `0`, `0xFFFFFFFF`와 올바른 cleanup을 반환합니다.
- facade mapping은 최종 R/RX이며 W+X page가 없습니다.
- Windows x86/x64, Linux x86/x64 관련 build와 unit test, Linux i386 runtime probe가 통과합니다.
- 설계·작업 로그와 커밋을 남깁니다.

*Acceptance requires descriptor/facade agreement for all four confirmed APIs; a registry `kernel32` handle equal to the real facade base with every export inside the image; Linux i386 facade calls for `GetVersion` and `CreateFileA` completing through the existing bridge with `0`, `0xFFFFFFFF`, and correct cleanup; final R/RX mappings with no W+X page; passing relevant Windows x86/x64 and Linux x86/x64 builds and unit tests plus the Linux i386 runtime probe; and a design, work log, and commit.*

## 제외 범위 / Out of scope

작업 344의 pseudo handle 제거, resolver 이관, static/dynamic address identity와 실제 4th CHD 회귀는 포함하지 않습니다. guest handle/VFS 동작, 다른 DLL, Linux x64 trampoline과 Windows adapter도 포함하지 않습니다.

*This task excludes Task 344's pseudo-handle removal, resolver migration, static/dynamic address identity, and real-4th-CHD regression. Guest-handle/VFS behavior, other DLLs, the Linux x64 trampoline, and the Windows adapter are also excluded.*
