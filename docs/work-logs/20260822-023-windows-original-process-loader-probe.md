# Windows Original-Process Loader Probe Result

## 한국어

`re2dj_windows_original_process_probe --hdd .\roms\ez2dj1stse`를 실행했습니다. x64 host가 원본 `ez2dj1.exe`를 `CREATE_SUSPENDED`로 생성했고, guest thread를 resume하지 않은 채 WOW64 PEB image-base field를 읽었습니다.

```json
{"target":"ez2dj1stse_unpacked","image_base":"0x00400000","main_module_base":"0x00400000","suspended":true}
```

처음 사용한 `EnumProcessModulesEx`는 suspended WOW64 process에서 `ERROR_PARTIAL_COPY (299)`를 반환했습니다. probe는 Microsoft가 문서화한 `ProcessWow64Information`을 통해 PEB 주소를 얻는 제한된 검증 경로로 변경했고, live result로 image base 일치를 확인했습니다.

결과적으로 manual mapper backend는 fixed-base image에서 중단하지만, original-process loader backend 후보는 계속 진행할 근거가 생겼습니다. 다음 작업은 injection 없이 suspended child의 import/IAT 주소를 원격으로 검증하는 단계입니다.

검증:

1. `cmake --build --preset windows-x64-debug --target re2dj_windows_original_process_probe` 성공
2. 실제 HDD probe가 위 JSON과 exit code 0을 반환

## English

`re2dj_windows_original_process_probe --hdd .\roms\ez2dj1stse` was run. The x64 host created the original `ez2dj1.exe` with `CREATE_SUSPENDED` and read its WOW64 PEB image-base field without resuming the guest thread.

```json
{"target":"ez2dj1stse_unpacked","image_base":"0x00400000","main_module_base":"0x00400000","suspended":true}
```

The initial `EnumProcessModulesEx` attempt returned `ERROR_PARTIAL_COPY (299)` for the suspended WOW64 process. The probe was changed to a bounded verification path using the Microsoft-documented `ProcessWow64Information` PEB address, and the live result confirmed matching image bases.

The manual-mapper backend remains blocked for fixed-base images, while the original-process loader backend candidate has evidence to continue. The next task remotely verifies import/IAT addresses in the suspended child without injecting code.

Verification:

1. `cmake --build --preset windows-x64-debug --target re2dj_windows_original_process_probe` succeeded.
2. The live HDD probe returned the JSON above with exit code 0.
