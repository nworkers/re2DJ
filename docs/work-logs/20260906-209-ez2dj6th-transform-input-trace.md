# ez2dj6th transform 입력 trace 작업 로그

## 한국어

### 결과

`--hardlock-transform-inputs` 진단 옵션을 추가하고 올바른 6th CHD 실행에서 확인했습니다.

- `0x458` transform descriptor가 실제로 도달합니다.
- descriptor function은 `0x0011`입니다.
- 입력 block 수는 7개이고 block 크기는 8바이트입니다.
- 기존 42-entry candidate map의 key hash와 실제 입력 hash 집합은 교집합이 없습니다.
- 따라서 기존 194개 map을 다시 sweep하는 것만으로는 seed를 판단할 수 없습니다.

원시 block은 기록하지 않고 child VFS trace에 FNV-1a 64-bit hash만 남겼습니다. 확인 로그는 `20260906-175433-575.child.vfs.log`입니다.

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1374, failures: 0`

## English

### Result

Added `--hardlock-transform-inputs` and verified it against the correct 6th CHD execution.

- The child reaches the `0x458` transform descriptor.
- The descriptor function is `0x0011`.
- There are seven input blocks, each eight bytes.
- The actual input hash set has no intersection with the keys in the existing 42-entry candidate maps.
- Repeating the existing 194-map sweep cannot judge the seeds.

Raw blocks were not placed in the trace; only FNV-1a 64-bit hashes were recorded. The verification trace is `20260906-175433-575.child.vfs.log`.

### Verification

- `cmd /c scripts\build_win32.bat` succeeded.
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1374, failures: 0`.
