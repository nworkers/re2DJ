# 작업 367 작업 로그 — envelope의 두 번째 층과 원본 진입 / Task 367 work log — Envelope second layer and hand-off to the original

설계: [20260925-367-envelope-handoff-to-original.md](../design/20260925-367-envelope-handoff-to-original.md)
작업 지시서: [20260925-367-envelope-handoff-to-original.md](../work-orders/20260925-367-envelope-handoff-to-original.md)

## 조사 / Investigation

실험 build에 두 가지 임시 장치를 달았다. 하나는 환경 변수가 가리키는 파일에서 해석 전용 export를 읽는 장치다. 다른 하나는 정지할 때 image를 dump하는 장치다. 미해석 lookup마다 이름을 더하는 loop로 envelope를 따라갔다. GDI32에서 새 module이 필요해진 시점에 복호화된 image를 dump했다. 그 `.idata`에서 나머지 여덟 DLL의 import 41개와 GDI32 12개를 읽어 한 번에 넣었다. 두 장치와 dump는 커밋하지 않았다(dump는 scratchpad에만 있다). 첫 시도에서 `VirtualProtect(ExitProcess thunk)`가 facade가 region에 없어 실패했다. 그러자 게스트는 `ReadProcessMemory`로 넘어갔다. facade를 region으로 등록하자 보호 변경과 hook 설치가 그대로 진행됐다.

*The experimental build carried two temporary aids: one read resolve-only exports from a file named by an environment variable, the other dumped the image at the stop. A loop followed the envelope, adding a name at every unresolved lookup. When a new module became necessary at GDI32, the decrypted image was dumped, and its `.idata` gave the remaining eight DLLs' 41 imports and GDI32's 12 at once. Neither aid nor the dump was committed (the dump stays in the scratchpad). In the first attempt `VirtualProtect(ExitProcess thunk)` failed because the facade was in no region, and the guest moved to `ReadProcessMemory`; with facades registered as regions, the protection change and hook installation went straight through.*

## 변경 / Changes

- **`resolve_only_modules.h/.cpp`**(새 파일): `ResolveOnlyExport`, `AddResolveOnlyExports`, `MakeResolveOnlyModuleDescriptors`(facade 7개, export 40개).
  ***`resolve_only_modules.h/.cpp`** (new): `ResolveOnlyExport`, `AddResolveOnlyExports`, and `MakeResolveOnlyModuleDescriptors` (7 facades, 40 exports).*
- **kernel32**: 구현 export 24개와 해석 전용 72개. `ReadProcessMemory`와 `WriteProcessMemory`를 구현했다(오류 299 추가). **user32**: `SetTimer`와 해석 전용 34개. **advapi32**: 해석 전용 4개(`RegFlushKey` 추가).
  ***kernel32:** 24 implemented and 72 resolve-only exports, with `ReadProcessMemory`/`WriteProcessMemory` implemented (error 299 added). **user32:** `SetTimer` plus 34 resolve-only. **advapi32:** 4 resolve-only (adding `RegFlushKey`).*
- **`GuestProcess`**: `Accessible`과 thread timer(`SetThreadTimer`, `timers`)를 추가했다. **`GuestModuleRegistry::modules()`**.
  ***`GuestProcess`:** adds `Accessible` and thread timers (`SetThreadTimer`, `timers`); **`GuestModuleRegistry::modules()`**.*
- **Linux**: 새 facade 일곱 개를 등록하고, 모든 facade를 PE header의 섹션과 함께 image region으로 등록한다. guest byte 범위는 `Accessible`을 따른다. facade code section은 RWX다. `native_guest_module_probe`는 "R header, RWX code"를 확인하며, 쓰지 않게 된 W+X 검사 함수는 지웠다.
  ***Linux:** registers the seven new facades and every facade as an image region with its PE header's sections; guest byte ranges follow `Accessible`; facade code sections are RWX, and `native_guest_module_probe` checks "R headers, RWX code", dropping its now-unused W+X helper.*
- **단위 테스트.** kernel32·user32·advapi32 export 수, 해석 전용 handler, 자기 process 복사(읽기 전용 page 쓰기, region 밖, 모르는 handle), thread timer(새 ID, 교체, 주기 하한, window timer 거절), 해석 전용 module 일곱 개(ordinal 포함), `Accessible`(NOACCESS·decommit 제외)을 검사한다.
  ***Unit tests** cover the kernel32/user32/advapi32 export counts and resolve-only handlers, own-process copies (a write to a read-only page, outside every region, an unknown handle), thread timers (new ID, replacement, period floor, window timer rejected), the seven resolve-only modules with ordinals, and `Accessible` (excluding NOACCESS and decommitted pages).*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음(trace 43 frame) / as before (43-frame trace) |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 호출 1,579번, 주소를 정규화하면 두 폭이 같다. Hardlock은 이전과 같다(descriptor 37, transform 36). `#1578 GetVersion`(`0x004c4424`) 뒤 `#1579 HeapCreate(1, 0x1000, 0)`에서 정지 / 1,579 calls, identical after address normalization; Hardlock as before (37 descriptors, 36 transforms); after `#1578 GetVersion` (`0x004c4424`) the run stops at `#1579 HeapCreate(1, 0x1000, 0)` |

## 다음 / Next

원본 프로그램의 MSVC CRT 시작(`HeapCreate`, heap, 환경, 명령줄, `GetStartupInfoA` 등)과, 그 뒤 게임 초기화를 차례로 제공한다.

*Provide the original program's MSVC CRT start-up (`HeapCreate`, the heap, environment, command line, `GetStartupInfoA`, and so on), then the game's initialization.*
