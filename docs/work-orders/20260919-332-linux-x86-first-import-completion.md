# 작업 지시 332: Linux x86 첫 import completion 관측 / Work order 332: Linux x86 first-import completion observation

실제 PE file을 받는 Linux x86 in-process diagnostic probe를 추가합니다. 확인된 첫 `GetModuleHandleA("kernel32")` argument와 gate를 검사하고, 4-byte cleanup 및 일회성 guest-memory breakpoint로 caller 복귀 직후 SIGTRAP을 관측합니다. 원본 자산은 읽기만 하며 product CLI와 IPC fallback은 변경하지 않습니다.

*Add a Linux x86 in-process diagnostic probe that accepts a real PE file. It checks the confirmed first `GetModuleHandleA("kernel32")` argument and gate, then observes SIGTRAP immediately after caller return through four-byte cleanup and a one-time guest-memory breakpoint. Original assets are read-only; product CLI and IPC fallback remain unchanged.*
