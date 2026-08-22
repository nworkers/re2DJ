# Windows Suspended IAT Probe Result

## 한국어

`re2dj_windows_original_process_probe --hdd .\roms\ez2dj1stse --verify-iat`를 실행했습니다. 원본 main image는 계속 `0x00400000`에 배치되었지만, `CREATE_SUSPENDED` 직후 IAT는 아직 loader-resolved 상태가 아니었습니다.

첫 실패 slot은 다음과 같습니다.

```text
KERNEL32.dll, IAT RVA 0x01aba354, value 0x01aba6ea
```

값 `0x01aba6ea`는 원본 image 범위 안에 있습니다. 따라서 이는 외부 `KERNEL32` 함수 주소가 아니라 import lookup thunk/힌트-이름 데이터로 해석됩니다. suspended process는 main image mapping 뒤, import binding을 포함한 loader initialization 이전에 멈춥니다.

이 probe는 읽기 전용이며 `VirtualProtectEx`, `WriteProcessMemory`, DLL injection, thread resume을 호출하지 않았습니다. IAT patch는 이 시점에서 수행하면 안 됩니다. 다음 후보는 debugger initial breakpoint에서 loader initialization 완료와 guest entry 이전을 구분하는 관찰입니다.

검증:

1. `cmake --build --preset windows-x64-debug --target re2dj_windows_original_process_probe` 성공
2. 실제 HDD IAT probe가 위의 unresolved slot을 재현

## English

`re2dj_windows_original_process_probe --hdd .\roms\ez2dj1stse --verify-iat` was run. The original main image remained mapped at `0x00400000`, but immediately after `CREATE_SUSPENDED` its IAT was not yet loader-resolved.

The first failing slot was:

```text
KERNEL32.dll, IAT RVA 0x01aba354, value 0x01aba6ea
```

Value `0x01aba6ea` lies inside the original image range. It is therefore interpreted as import lookup-thunk/hint-name data rather than an external `KERNEL32` function address. A suspended process stops after main-image mapping and before loader initialization, including import binding.

This probe is read-only and never calls `VirtualProtectEx`, `WriteProcessMemory`, DLL injection, or thread resume. IAT patching must not occur at this point. The next candidate observes the debugger initial breakpoint to distinguish loader-initialization completion from guest entry.

Verification:

1. `cmake --build --preset windows-x64-debug --target re2dj_windows_original_process_probe` succeeded.
2. The live HDD IAT probe reproduced the unresolved slot above.
