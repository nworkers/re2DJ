# 작업 로그 324: Linux 첫 import 인자 텍스트 관찰 / Work log 324: Linux first-import argument text observation

## 결과 / Result

`RunOriginalUntilBoundary()`는 first known import의 stack return slot과 arg0를 읽은 다음, arg0가 0이 아니면 `native_protocol::kMaximumImportStringSize`인 4096바이트를 한 번 읽습니다. 첫 NUL까지의 raw bytes를 결과 텍스트로 보관합니다. read 실패 또는 4096바이트 안의 NUL 부재는 helper 중지와 실행 오류가 됩니다.

*`RunOriginalUntilBoundary()` reads the first known import's stack return slot and arg0, then makes one 4096-byte read, `native_protocol::kMaximumImportStringSize`, when arg0 is nonzero. It retains raw bytes through the first NUL as result text. A failed read or absent NUL within 4096 bytes stops the helper and becomes an execution error.*

## 실제 관찰 / Actual observation

WSL Ubuntu 24.04에서 로컬 `ez2dj4th` CHD와 공유 ELF32 i386 helper를 사용한 Linux x64와 clean-rebuilt Linux x86 실행은 모두 다음을 보고했습니다.

*Linux x64 and clean-rebuilt Linux x86 execution on WSL Ubuntu 24.04, using the local `ez2dj4th` CHD and shared ELF32 i386 helper, both reported the following.*

```
first boundary  : import kernel32.dll!GetModuleHandleA
stack ret / arg0: 0x00ae028a / 0x00ae0f2c
arg0 text       : kernel32
```

`kernel32`은 bounded byte observation입니다. 이 작업은 API argument semantics, module lookup, handle 반환, stack cleanup 또는 original-code continuation을 구현하거나 확인하지 않았습니다.

*`kernel32` is a bounded byte observation. This task neither implements nor establishes API argument semantics, module lookup, handle return, stack cleanup, or original-code continuation.*

## 검증 / Validation

- `cmake --build --preset linux-x64-debug` 및 실제 `ez2dj4th --run --linux-helper ...`: 통과.
- `ctest --test-dir build/linux-x64-debug --output-on-failure`: 1/1 통과.
- `cmake --build --preset linux-x86-debug --clean-first` 및 실제 `ez2dj4th --run --linux-helper ...`: 통과.
- `ctest --test-dir build/linux-x86-debug --output-on-failure`: 1/1 통과.

*`cmake --build --preset linux-x64-debug` and actual `ez2dj4th --run --linux-helper ...` passed. Linux x64 CTest passed 1/1. `cmake --build --preset linux-x86-debug --clean-first` and actual Linux x86 execution passed. Linux x86 CTest passed 1/1.*
