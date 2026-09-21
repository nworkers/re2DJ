# 작업 로그 327: Linux native PE image 수명주기 분리 / Work log 327: Linux native PE image lifecycle extraction

## 결과 / Result

PE32 mapping, section copy, HIGHLOW relocation, unmap을 `NativePeImage`과 `MapNativePe32Image()`/`ReleaseNativePeImage()`로 Linux platform component에 분리했습니다. i386 helper는 새 component를 사용하며, import thunk binding과 guest bootstrap은 기존 순서로 mapping 결과를 사용합니다.

*Extracted PE32 mapping, section copy, HIGHLOW relocation, and unmap into Linux platform component `NativePeImage` with `MapNativePe32Image()`/`ReleaseNativePeImage()`. The i386 helper uses the new component, while import-thunk binding and guest bootstrap consume mapping results in their existing order.*

이 작업은 in-process execution backend를 아직 추가하지 않으며, protocol, import completion, dynamic memory, original target 실행 의미를 바꾸지 않습니다.

*This task does not yet add an in-process execution backend and does not change protocol, import completion, dynamic memory, or original-target execution semantics.*

## 검증 / Validation

WSL Ubuntu 24.04에서 `bash scripts/test_linux_native_helper_probe.sh`를 실행했습니다. x64/x86 CTest가 각각 1/1 통과했고, 공유 ELF32 helper를 통한 normal import (`result=51 child=0`), SIGILL fault, terminal stop, capability rejection이 모두 통과했습니다.

*Ran `bash scripts/test_linux_native_helper_probe.sh` on WSL Ubuntu 24.04. x64/x86 CTest each passed 1/1, and normal import (`result=51 child=0`), SIGILL fault, terminal stop, and capability rejection all passed through the shared ELF32 helper.*
