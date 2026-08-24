# x86 I/O 포트 명령과 사용자 모드 trap

x86 `IN`과 `OUT`은 port I/O 주소 공간과 accumulator 사이에서 1, 2, 4바이트를 전송한다. `DX` 형식은 16비트 port 번호를 사용한다. Intel 명령 참조는 이 명령과 I/O privilege 검사를 정의한다. 사용자 모드에서 권한이 없는 직접 port I/O는 정상 명령처럼 실행할 수 없으므로 실행 backend가 장치 HLE 경계로 삼을 수 있다.

근거: [Intel 64 and IA-32 Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)

Windows debugger는 하드웨어 exception을 debug event로 받을 수 있다. debugger는 주소와 opcode를 검증하고 thread context에 결과를 반영한 뒤 정확한 명령 길이만큼 instruction pointer를 진행해야 한다. `DBG_CONTINUE`는 예외를 처리된 것으로 표시하므로 알 수 없는 주소·opcode·port는 `DBG_EXCEPTION_NOT_HANDLED`로 전달해야 한다.

근거: [Debugger Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/debugger-exception-handling), [ContinueDebugEvent](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-continuedebugevent)

이 방식은 import thunk가 없는 직접 하드웨어 명령에만 제한한다. 일반 Win32 서비스는 import 경계를 우선 사용하고, port trap은 target별 주소·width·허용 port로 좁힌다.

---

# x86 I/O Port Instructions and User-Mode Traps

x86 `IN` and `OUT` transfer one, two, or four bytes between the port-I/O address space and the accumulator; the `DX` forms use a 16-bit port number. Intel defines their I/O privilege checks. Direct user-mode port I/O without sufficient privilege can therefore form a device-HLE boundary.

A Windows debugger can receive the hardware exception as a debug event. It must validate the address and opcode, update the thread context, and advance by exactly one decoded instruction. Because `DBG_CONTINUE` marks the exception handled, unknown addresses, opcodes, widths, and ports must remain unhandled. This trap is reserved for direct hardware instructions with no import-thunk alternative.

References: [Intel manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), [Microsoft debugger exception handling](https://learn.microsoft.com/en-us/windows/win32/debug/debugger-exception-handling), [Microsoft ContinueDebugEvent](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-continuedebugevent)
