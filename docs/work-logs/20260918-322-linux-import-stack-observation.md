# 작업 로그 322: Linux 첫 import 스택 관찰 / Work log 322: Linux first-import stack observation

## 결과 / Result

Linux runner는 알려진 import gate를 수신하면 completion을 보내기 전에 pending guest stack의 8바이트를 읽습니다. little-endian 첫 워드는 `import_return_address`, 다음 워드는 `import_first_argument`으로 결과에 보관하며, 읽기 오류는 helper를 중지한 뒤 실행 오류가 됩니다. CHD와 directory HDD Linux 출력은 이 관찰값을 import 이름과 함께 표시합니다.

*When the Linux runner receives a known import gate, it reads eight bytes from the pending guest stack before sending completion. It stores the little-endian first word as `import_return_address` and the next word as `import_first_argument`; a read error stops the helper and becomes an execution error. Linux CHD and directory-HDD output display these observations beside the import name.*

## 실제 관찰 / Actual observation

WSL Ubuntu 24.04에서 로컬 `ez2dj4th` CHD와 공유 ELF32 i386 helper를 사용했습니다. Linux x64 build와 clean-rebuilt Linux x86 build는 모두 다음 첫 경계를 보고했습니다.

*Used the local `ez2dj4th` CHD and the shared ELF32 i386 helper on WSL Ubuntu 24.04. Both the Linux x64 build and a clean-rebuilt Linux x86 build reported this first boundary.*

```
load base       : 0x00400000
entry point     : 0x00ae0240
first boundary  : import kernel32.dll!GetModuleHandleA
stack ret / arg0: 0x00ae028a / 0x00ae0f2c
```

이는 return slot과 첫 caller word의 관찰만 뜻합니다. `arg0`의 byte contents, module 이름, `GetModuleHandleA`의 반환값 및 이후 protection stub 진행은 확인하지 않았습니다.

*This means only that the return slot and first caller word were observed. The byte contents at `arg0`, module name, `GetModuleHandleA` return value, and later protection-stub progress were not verified.*

## 검증 / Validation

- `cmake --build --preset linux-x64-debug`: 통과.
- `cmake --build --preset linux-x86-debug --clean-first`: 통과.
- Linux x64 및 x86 실제 `ez2dj4th --run --linux-helper ...`: 위와 같은 값으로 통과.
- `ctest --test-dir build/linux-x64-debug --output-on-failure`: 1/1 통과.
- `ctest --test-dir build/linux-x86-debug --output-on-failure`: 1/1 통과.
- `git diff --check`: 공백 오류 없음. 기존 CRLF 정규화 경고만 보고됨.

*`cmake --build --preset linux-x64-debug` passed. `cmake --build --preset linux-x86-debug --clean-first` passed. Actual Linux x64 and x86 `ez2dj4th --run --linux-helper ...` passed with the values above. CTest passed 1/1 for each product build. `git diff --check` found no whitespace errors and reported only existing CRLF normalization warnings.*

기존 `re2dj_linux_native_ipc_host_probe`는 protected-page fixture가 쓴 `0x05`를 다음 import의 persistent-byte 비교 전에 `0x09`로 복원하지 않아 현재 종료 코드 1로 실패합니다. 이번 runner 코드가 이 fixture나 backend protocol을 변경하지는 않았지만, L2 검증을 다시 녹색으로 만들기 위한 별도 후속 작업이 필요합니다.

*The existing `re2dj_linux_native_ipc_host_probe` currently exits with code 1 because its protected-page fixture does not restore written `0x05` to `0x09` before the next import's persistent-byte comparison. This runner change did not alter that fixture or the backend protocol, but a separate follow-up is required to restore the L2 validation to green.*
