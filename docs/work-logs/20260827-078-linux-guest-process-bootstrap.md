# Linux guest process bootstrap 작업 로그

## 결과

Linux i386 helper에 `NativeProcessBootstrap` subsystem을 추가했다. 이 subsystem은 아래쪽 guard page가 있는 1 MiB guest stack, 별도 page의 최소 x86 TEB/PEB, `set_thread_area`로 만든 FS descriptor, 64 KiB alternate signal stack과 guest signal handler lifetime을 소유한다. TEB에는 exception-list sentinel, 실제 stack base/limit, self pointer, PEB pointer와 0인 last-error를 두고 PEB에는 mapped image base를 기록한다.

TLS process-attach callback과 PE entry는 naked i386 bridge를 통해 guest stack으로 전환한 뒤 실행한다. guest 실행 직전에 FS를 guest TEB selector로 바꾸고 control code로 복귀하면 기존 Linux FS를 복원한다. helper의 IPC와 mapping 코드는 계속 원래 control stack에서 실행한다.

guest 실행 중 `SIGSEGV`, `SIGBUS`, `SIGILL`, `SIGFPE`, `SIGTRAP`이 발생하면 alternate signal stack handler가 ucontext의 EIP/ESP와 signal 번호만 저장하고 `siglongjmp`로 control stack에 복귀한다. pipe I/O는 signal handler 밖에서 수행하며 기존 protocol v3 `ExecutionEvent`를 `kFault`로 전송한다. fault 뒤 guest 실행은 재개하지 않는다.

synthetic TLS callback은 `FS:[0x18]` self pointer와 TEB stack base/limit 안의 ESP를 검사한 경우에만 state 7을 기록한다. Linux 전용 두 번째 fixture는 `FS:[0x30]` PEB의 image base를 확인하고 `UD2`를 실행한다.

## 검증

- WSL Ubuntu 24.04 i386 warnings-as-errors helper build 성공.
- Linux x64 synthetic host probe build 성공.
- 기존 non-preferred mapping·relocation·TLS·named/ordinal import·memory IPC 회귀 성공: result 51, child 0.
- fault fixture 성공: `SIGILL` 4, EIP `0x1000100f`, nonzero guest ESP 관찰.
- Windows Win32 공용 코어 build와 CTest 2/2 성공. Windows native helper target도 기존 SDL3 source cache와 audio-off probe 구성에서 컴파일됐다.
- 원본 HDD 자산은 읽거나 수정하지 않았다.

## 회고

fault fixture의 첫 시도는 기존 relocation entry가 교체한 entry 기계어 위치에도 적용되어 event 전에 helper가 종료됐다. fixture를 preferred base에 mapping해 relocation delta를 0으로 만들고 PEB 검증과 fault 검증을 분리하지 않은 채 정확한 의도를 유지했다. protocol 확장 없이 이미 예약된 fault event field를 활용할 수 있었고, 다음 단계는 이 안정된 process context 위에 공용 import dispatcher와 ABI marshalling을 올리는 것이다.

---

# Linux Guest Process Bootstrap Work Log

## Result

Added a Linux-i386 `NativeProcessBootstrap` subsystem. It owns a 1 MiB guest stack with a lower guard page, minimal x86 TEB/PEB pages, an FS descriptor allocated with `set_thread_area`, a 64 KiB alternate signal stack, and guest signal-handler lifetime. The TEB contains the exception-list sentinel, actual stack base and limit, self pointer, PEB pointer, and zero last-error; the PEB records the mapped image base.

Process-attach TLS callbacks and PE entry code switch to the guest stack through naked i386 bridges. FS changes to the guest TEB selector immediately before guest execution and returns to the prior Linux selector on the control stack. Mapping and IPC continue on the helper's original control stack.

During guest execution, `SIGSEGV`, `SIGBUS`, `SIGILL`, `SIGFPE`, and `SIGTRAP` handlers capture only signal number and ucontext EIP/ESP on the alternate stack, then return through `siglongjmp`. Pipe I/O happens outside the signal handler using the existing protocol-v3 `ExecutionEvent` as `kFault`. Guest execution does not resume after a fault.

The synthetic TLS callback writes state 7 only after validating `FS:[0x18]` self and ESP within the TEB stack bounds. A second Linux-only fixture validates the `FS:[0x30]` PEB image base and executes `UD2`.

## Verification

- The WSL Ubuntu 24.04 warnings-as-errors i386 helper build passed.
- The Linux x64 synthetic host probe build passed.
- The existing non-preferred mapping, relocation, TLS, named/ordinal import, and memory-IPC regression passed with result 51 and child exit 0.
- The fault fixture observed `SIGILL` 4, EIP `0x1000100f`, and a nonzero guest ESP.
- The Windows Win32 shared-core build and CTest passed 2/2. The Windows native-helper target also compiled using the existing SDL3 source cache with audio disabled for the probe configuration.
- No original HDD assets were read or modified.

## Retrospective

The first fault-fixture attempt reused relocation entries whose operand locations had been replaced by new entry machine code, causing the helper to exit before an event. Mapping that fixture at its preferred base made the relocation delta zero while retaining a combined PEB-and-fault check. No protocol expansion was required because the existing fault-event fields were sufficient. The next phase can build shared import dispatch and ABI marshalling on this stable process context.
