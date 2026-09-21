# 작업 로그 326: Linux helper process-entry 분리 / Work log 326: Linux helper process-entry separation

## 결과 / Result

기존 `native_ipc_helper.cpp`의 `main()` body를 `RunNativeIpcHelper()`로 옮기고, 새 `native_ipc_helper_main.cpp`가 i386 assertion과 호출만 담당하게 했습니다. helper protocol, exit status, PE mapping, bootstrap, import thunk, dynamic memory 정책은 바꾸지 않았습니다.

*Moved the existing `main()` body in `native_ipc_helper.cpp` into `RunNativeIpcHelper()`, and made new `native_ipc_helper_main.cpp` own only the i386 assertion and call. Helper protocol, exit status, PE mapping, bootstrap, import thunk, and dynamic-memory policy are unchanged.*

이것은 in-process backend가 아직 아닙니다. 다음 작업은 callable body의 protocol adapter와 native PE execution session을 분리해 Linux x86 product가 pipe 없이 session을 호출할 수 있게 만드는 것입니다.

*This is not yet an in-process backend. The next task separates protocol adapter from native PE execution session so the Linux x86 product can call session without pipes.*

## 검증 / Validation

WSL Ubuntu 24.04에서 `bash scripts/test_linux_native_helper_probe.sh`를 실행했습니다. x64/x86 CTest가 각각 1/1 통과했고, 공유 ELF32 helper를 사용한 normal import probe (`result=51 child=0`), SIGILL fault, terminal stop, capability rejection이 모두 통과했습니다.

*Ran `bash scripts/test_linux_native_helper_probe.sh` on WSL Ubuntu 24.04. x64/x86 CTest each passed 1/1, and normal import probe (`result=51 child=0`), SIGILL fault, terminal stop, and capability rejection all passed with the shared ELF32 helper.*
