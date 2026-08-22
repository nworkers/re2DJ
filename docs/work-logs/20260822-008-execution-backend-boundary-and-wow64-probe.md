# 작업 로그: 실행 backend 경계와 WOW64 probe

## 결과

공용 `ExecutionBackend` event/reply 인터페이스를 추가했습니다. 이미지 준비, 실행 시작, event 대기, import 완료, 중단 요청을 backend 구현과 분리하며 host pointer나 OS handle을 공개하지 않습니다. import event와 응답은 backend-local thread/event ID, guest EIP/ESP, gate 주소, EAX/EDX, callee stack 정리 byte 수를 전달하므로 향후 32/64비트 IPC와 멀티스레드 guest를 표현할 수 있습니다.

Windows 전용 Win32 x86 native helper probe도 추가했습니다. probe는 실행 중 x86 machine code를 생성하고 `__stdcall` C++ gate를 호출합니다. x64 Windows에서 WOW64 실행 여부를 확인하며 gate 호출 횟수와 반환값이 일치할 때만 성공합니다.

## Result

Added the shared `ExecutionBackend` event/reply interface. It separates image preparation, start, event waiting, import completion, and stop requests without exposing host pointers or OS handles. Import events and replies carry backend-local event/thread IDs, guest EIP/ESP, gate addresses, EAX/EDX, and callee-cleaned stack bytes, providing the shape needed for later 32/64-bit IPC and multithreaded guests.

Also added a Windows-only Win32 x86 native-helper probe. It emits x86 machine code at runtime and calls a `__stdcall` C++ gate, confirms WOW64 execution on x64 Windows, and succeeds only when the gate count and return value match.

## 확인 결과

probe 출력:

```text
native-helper-probe: x86=yes wow64=yes gate-calls=1 result=42
```

따라서 Windows x64에서 별도 Win32 프로세스를 네이티브 x86 실행 주체로 사용하고 같은 프로세스의 HLE gate 함수로 진입하는 경로는 실현 가능합니다. 아직 확인하지 않은 것은 synthetic PE32를 선호 주소에 mapping하는 경로와 64비트 host service까지의 IPC 왕복입니다.

## Finding

The probe output was `native-helper-probe: x86=yes wow64=yes gate-calls=1 result=42`. A separate Win32 process can therefore serve as the native x86 execution subject on Windows x64 and enter an in-process HLE gate. Synthetic PE32 mapping at its preferred base and the IPC round trip to 64-bit host services remain unverified.

## 검증

* `cmake --preset windows-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` — 성공
* `cmake --build --preset windows-x64-debug` — 성공
* `ctest --preset windows-x64-debug` — 1/1 통과
* `powershell -ExecutionPolicy Bypass -File scripts/test_windows_native_helper_probe.ps1` — 성공
* Win32 probe CTest — 1/1 통과
* `git diff --check` — 성공

## Verification

The Windows x64 warnings-as-errors configure/build and unit suite passed. The Win32 probe configured and built with warnings as errors, passed its CTest under WOW64, and `git diff --check` reported no whitespace errors.
