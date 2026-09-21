# Linux x86 resolver-continuation instruction trace / Linux x86 resolver-continuation instruction trace

## 목적 / Purpose

4th Trax의 최소 resolver-continuation 실행은 동적 GetVersion thunk 호출 전에 0x00af0c22의 null read로 멈춥니다. Task 337은 fault 전후의 바이트와 즉시 분기를 확인했지만, 실제로 어떤 제어 흐름을 거쳐 EAX가 0이 되었는지는 확정하지 못했습니다.

*The 4th Trax minimum resolver-continuation run stops on a null read at 0x00af0c22 before calling the dynamic GetVersion thunk. Task 337 confirmed bytes around the fault and its immediate branch, but could not establish the actual control flow that produced EAX=0.*

Linux i386 전용 진단은 GetProcAddress(kernel32, GetVersion)가 정상적으로 반환할 caller 주소의 원래 첫 바이트만 process-local INT3로 바꿉니다. breakpoint에 도달하면 원래 바이트를 복원하고 EIP를 되감은 뒤 trap flag를 설정합니다. 이후 각 single-step trap에서 EIP, ESP, 범용 레지스터와 EFLAGS를 고정 크기 배열에 기록합니다. fault, 정상 종료, 또는 128개 frame 한도에서 추적을 종료합니다.

*The Linux i386-only diagnostic replaces only the original first byte of the caller address to which GetProcAddress(kernel32, GetVersion) normally returns with a process-local INT3. At the breakpoint, it restores that byte, rewinds EIP, and sets the trap flag. Each subsequent single-step trap records EIP, ESP, general registers, and EFLAGS in a fixed array. Tracing ends on a fault, normal exit, or a 128-frame limit.*

~~~mermaid
sequenceDiagram
    participant G as Original guest
    participant B as import bridge
    participant T as trace controller
    participant S as Linux signal handler

    G->>B: GetProcAddress(kernel32, GetVersion)
    B->>T: arm INT3 at guest return address
    B-->>G: executable dynamic thunk pointer
    G->>S: INT3 at return address
    S->>T: restore byte, rewind EIP, set TF
    loop at most 128 instructions
        G->>S: single-step SIGTRAP
        S->>T: append register frame
        S-->>G: continue with TF
    end
    G->>S: fault or trace limit
    S->>T: preserve trace and terminal state
~~~

## 범위와 안전 경계 / Scope and safety boundary

이 기능은 --linux-in-process-getversion-call 경로에서만 arm하며 Linux i386 host에서만 동작합니다. 일반 helper와 제품 실행 경로, HLE 반환값, 원본 HDD/CHD는 변경하지 않습니다. trace는 동적으로 매핑된 process-local PE image 안의 단일 바이트만 잠시 변경하고, 시작되지 않은 경우와 실행이 끝나는 경우 모두 복원합니다. 이 기능은 호환성 구현이나 보호 우회가 아니라 이미 확인된 null read의 원인을 관측하기 위한 진단입니다.

*This feature arms only on the --linux-in-process-getversion-call path and only on a Linux i386 host. It does not alter the normal helper or product execution path, HLE return values, or the original HDD/CHD. The trace temporarily changes one byte only inside the dynamically mapped process-local PE image, restoring it whether tracing starts or execution ends. It is diagnostic observation for the already confirmed null read, not a compatibility implementation or a protection bypass.*

signal handler는 allocation, I/O, 문자열 처리, mutex를 수행하지 않습니다. 고정 크기 sig_atomic_t 저장소에만 기록하고, siglongjmp로 host 코드로 돌아온 뒤 일반 C++ 구조로 복사합니다. trace limit은 일반 fault와 별도 상태로 보존하여, limit에 닿은 결과를 fault로 해석하지 않습니다.

*The signal handler performs no allocation, I/O, string work, or mutex operation. It writes only to fixed-size sig_atomic_t storage, which is copied into normal C++ structures after control returns to host code through siglongjmp. A trace-limit result is preserved separately from an ordinary fault so a limit is not interpreted as a guest fault.*

## 검증 / Validation

1. synthetic i386 probe가 caller-return breakpoint에서 trace를 시작하고, UD2 fault 전 명령어 frame과 복원된 원래 바이트를 검증합니다.
2. Linux x86 product build와 probe를 실행합니다.
3. 사용자 제공 4th CHD에서 기존 진단을 실행하여 실제 trace와 terminal fault를 수집하고, 확인된 control flow만 analysis에 기록합니다.
4. Linux x64 product build가 i386 전용 진단을 명시적으로 거절하는지 확인합니다.

*1. The synthetic i386 probe verifies trace start at a caller-return breakpoint, instruction frames before its UD2 fault, and restoration of the original byte.
2. Build and run the Linux x86 product and probe.
3. Run the existing diagnostic on the user-provided 4th CHD, collect the actual trace and terminal fault, and record only confirmed control flow in analysis.
4. Verify that the Linux x64 product build explicitly rejects the i386-only diagnostic.*
## 구현 보완 / Implementation refinement

실제 trace에서 import bridge의 host 코드까지 trap flag가 전파되면 128-frame 한도가 guest fault보다 먼저 발생할 수 있음이 확인되었습니다. trace controller는 guest image 범위 밖의 첫 single-step frame을 기록한 뒤 TF를 해제하고, import bridge가 guest return slot에서 process-local breakpoint를 다시 arm하게 합니다.

*The real trace showed that propagating the trap flag into import-bridge host code can reach the 128-frame limit before the guest fault. The trace controller records the first single-step frame outside the guest image, clears TF, and lets the import bridge re-arm a process-local breakpoint at the guest return slot.*