# Linux x86 첫 import completion 관측 / Linux x86 first-import completion observation

## 목적 / Purpose

실제 `ez2dj4th` PE가 Linux x86 `NativeInProcessRunner`에서 첫 `kernel32!GetModuleHandleA("kernel32")` import를 완료한 뒤 원래 caller로 복귀하는 경계를 관측한다.

*Observe the boundary at which the real `ez2dj4th` PE completes its first `kernel32!GetModuleHandleA("kernel32")` import and returns to the original caller in Linux x86 `NativeInProcessRunner`.*

## 안전한 정지 방식 / Safe stop method

handler는 확인된 first gate와 ANSI argument만 받아들인다. 반환 register와 4-byte stdcall cleanup을 설정한 뒤 guest return address의 첫 byte를 process-local `INT3`로 바꾼다. thunk가 cleanup을 적용하고 원래 address로 jump하면 `NativeProcessBootstrap`의 기존 SIGTRAP boundary가 실행을 멈춘다. probe는 expected `return_address + 1` EIP와 기록된 handler result를 함께 검사한다.

*The handler accepts only the confirmed first gate and ANSI argument. After setting return registers and four-byte stdcall cleanup, it replaces the first byte at guest return address with a process-local `INT3`. Once the thunk applies cleanup and jumps to the original address, the existing SIGTRAP boundary in `NativeProcessBootstrap` stops execution. The probe checks expected `return_address + 1` EIP together with recorded handler result.*

guest mapping만 수정하며 원본 PE file, CHD, HDD asset은 바꾸지 않는다. `INT3` 이후 API는 실행하지 않으므로 module handle의 실제 장기 의미나 후속 `GetProcAddress` 호환을 주장하지 않는다.

*Only guest mapping is modified; the original PE file, CHD, and HDD asset are not changed. No API after `INT3` executes, so this does not claim long-term module-handle meaning or later `GetProcAddress` compatibility.*

## 미확정 사항 / Unresolved items

반환할 pseudo module handle 값과 실제 caller가 이를 사용하는 방식은 이 정지 범위에서 검증되지 않는다. 따라서 이 작업은 `GetModuleHandleA`의 완전 HLE binding이 아니라 import ABI completion 관측이다.

*The pseudo module-handle value and the way the real caller uses it are not validated within this stopping scope. This is import-ABI completion observation, not a complete `GetModuleHandleA` HLE binding.*
