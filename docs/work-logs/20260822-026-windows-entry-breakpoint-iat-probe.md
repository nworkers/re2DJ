# Windows Entry Hardware-Breakpoint IAT Probe Result

## 한국어

WOW64 primary thread의 `DR0`에 원본 entry `0x0043A640`을 설정했습니다. 첫 debugger breakpoint 뒤 `0x4000001F` WOW64 transition breakpoint를 계속 처리했고, entry 주소에서 `0x4000001E` WOW64 single-step으로 정지했습니다. guest entry 명령은 계속하지 않았습니다.

```json
{"target":"ez2dj1stse_unpacked","image_base":"0x00400000","main_module_base":"0x00400000","iat_slots":144,"iat_modules":7,"initial_breakpoint":true,"suspended":false}
```

IAT 144 slot과 7 DLL이 모두 nonzero 외부 함수 주소였습니다. 이 entry hardware-breakpoint stop은 suspended 및 첫 debugger breakpoint와 달리 loader import binding 이후, guest entry 이전의 검증된 IAT patch 후보입니다. 실제 patch·injection·resume은 아직 수행하지 않았습니다.

## English

Original entry `0x0043A640` was placed in `DR0` of the WOW64 primary thread. After the first debugger breakpoint, the `0x4000001F` WOW64 transition breakpoint was continued and execution stopped with WOW64 single-step `0x4000001E` at the entry address. The guest entry instruction was not continued.

```json
{"target":"ez2dj1stse_unpacked","image_base":"0x00400000","main_module_base":"0x00400000","iat_slots":144,"iat_modules":7,"initial_breakpoint":true,"suspended":false}
```

All 144 IAT slots across seven DLLs were nonzero external function addresses. Unlike suspended and first-debugger-breakpoint stops, this entry hardware-breakpoint stop is a verified IAT-patch candidate after loader import binding and before guest entry. No actual patch, injection, or resume was performed.
