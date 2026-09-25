# 작업 367 작업 지시서 — envelope의 두 번째 층과 원본 진입 / Task 367 work order — Envelope second layer and hand-off to the original

설계: [20260925-367-envelope-handoff-to-original.md](../design/20260925-367-envelope-handoff-to-original.md)

## 절차 / Steps

1. `ResolveOnlyExport`, `AddResolveOnlyExports`, `MakeResolveOnlyModuleDescriptors`를 추가한다. kernel32·user32·advapi32의 해석 전용 export를 이 표로 옮기고 늘린다.
   *Add `ResolveOnlyExport`, `AddResolveOnlyExports`, and `MakeResolveOnlyModuleDescriptors`, moving and extending the kernel32, user32, and advapi32 resolve-only exports onto the table.*
2. `GuestModuleRegistry::modules()`와 `GuestProcess::Accessible`을 추가한다. Linux 진단은 facade를 image region으로 등록하고, 새 facade 일곱 개를 등록한다.
   *Add `GuestModuleRegistry::modules()` and `GuestProcess::Accessible`; the Linux diagnostic registers facades as image regions and registers the seven new facades.*
3. facade code section을 host에서 RWX로 map하고, facade probe의 보호 검사를 바꾼다.
   *Map facade code sections RWX on the host and change the facade probe's protection check.*
4. `ReadProcessMemory`/`WriteProcessMemory`와 `SetTimer`(thread timer)를 구현한다. Win32 오류 299를 추가한다.
   *Implement `ReadProcessMemory`/`WriteProcessMemory` and `SetTimer` (thread timers), adding Win32 error 299.*
5. 단위 테스트, 분석·TODO·ARCHITECTURE 갱신.
   *Unit tests and analysis/TODO/ARCHITECTURE updates.*

## 완료 조건 / Done when

- Linux 두 폭과 Windows x86 build·CTest가 통과한다. 기존 진단·probe가 그대로다.
  *Both Linux widths and Windows x86 build and pass CTest, with existing diagnostics and probes unchanged.*
- 실제 4th가 두 폭에서 `GetVersion`(`0x004c4424`)을 지나 `HeapCreate`에서 멈춘다. 주소를 정규화한 호출 기록이 같다.
  *On both widths the real 4th passes `GetVersion` (`0x004c4424`) and stops at `HeapCreate`, with identical call records after address normalization.*
