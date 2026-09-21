# 작업 로그 338: Linux x86 resolver-continuation instruction trace / Work log 338: Linux x86 resolver-continuation instruction trace

## 결과 / Result

Linux i386 in-process 진단에 제한된 instruction trace controller를 추가했습니다. GetProcAddress(kernel32, GetVersion)의 guest caller return 주소에 process-local INT3를 설치하고, breakpoint에서 원래 바이트를 복원한 뒤 TF로 guest instruction frame을 수집합니다. 각 frame은 EIP, ESP, EAX/EBX/ECX/EDX/ESI/EDI/EBP, EFLAGS를 보존합니다.

*Added a bounded instruction-trace controller to the Linux i386 in-process diagnostic. It installs a process-local INT3 at the guest caller return address of GetProcAddress(kernel32, GetVersion), restores the original byte at the breakpoint, then collects guest instruction frames through TF. Each frame preserves EIP, ESP, EAX/EBX/ECX/EDX/ESI/EDI/EBP, and EFLAGS.*

처음 실제 실행에서는 TF가 import bridge의 host 코드까지 전파되어 128-frame limit가 guest fault보다 먼저 발생했습니다. controller는 이제 guest image 밖의 첫 frame에서 TF를 끄고 native import bridge가 guest return slot에 다음 breakpoint를 설치해 trace를 재개합니다.

*The first real run allowed TF to propagate into import-bridge host code and reached the 128-frame limit before the guest fault. The controller now clears TF at the first frame outside the guest image, and the native import bridge installs the next breakpoint at the guest return slot to resume tracing.*

실제 4th CHD run은 37 frame으로 0x00af0c22 null read까지 도달했습니다. 0x00af09f0 native import thunk 뒤 0x00af09f6에서 EAX가 0으로 복귀했고, handler는 해당 dynamic resolver 요청의 인자를 kernel32, CreateFileA로 확인했습니다. 이 최소 경로에서 미처리 CreateFileA resolver가 EAX=0의 직접 원인입니다. GetVersion thunk 호출은 여전히 이 지점 뒤이므로 확인되지 않았습니다.

*The real 4th CHD run reached the 0x00af0c22 null read in 37 frames. EAX returned as zero at 0x00af09f6 after the native import thunk at 0x00af09f0, and the handler confirmed kernel32, CreateFileA as the dynamic resolver arguments. In this minimum path, the unhandled CreateFileA resolver is the direct cause of EAX=0. The GetVersion thunk call remains later than this point and is still unobserved.*

## 검증 / Validation

- WSL Linux x86 Debug product build와 synthetic probe를 실행했습니다.
- synthetic probe 출력은 imports=2 dynamic=1 exit=51 signal=4였습니다.
- 실제 4th CHD 진단은 37 frames, limit=0, terminal SIGSEGV EIP 0x00af0c22, unhandled dynamic request CreateFileA를 기록했습니다.
- Linux x64 product build와 i386-only diagnostic rejection은 아래 검증에서 추가합니다.

*Validation performed:*

- *Ran the WSL Linux x86 Debug product build and synthetic probe.*
- *The synthetic probe reported imports=2 dynamic=1 exit=51 signal=4.*
- *The real 4th CHD diagnostic recorded 37 frames, limit=0, terminal SIGSEGV EIP 0x00af0c22, and unhandled dynamic request CreateFileA.*
- *The Linux x64 product build and i386-only diagnostic rejection are added below after that validation.*
- Linux x64 Debug: product target를 빌드했고, i386 전용 diagnostic은 Linux in-process GetVersion diagnostic requires an i386 host 오류와 함께 예상 exit 4로 거절되었습니다.

*Linux x64 Debug: built the product target, and the i386-only diagnostic was rejected with the expected exit 4 and the error Linux in-process GetVersion diagnostic requires an i386 host.*

- Linux x86 helper: linux-x86-helper preset의 re2dj_linux_native_ipc_helper를 다시 빌드했습니다.

*Linux x86 helper: rebuilt re2dj_linux_native_ipc_helper through the linux-x86-helper preset.*