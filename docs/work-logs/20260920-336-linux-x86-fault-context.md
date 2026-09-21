# 작업 로그 336: Linux x86 fault context 관측 / Work log 336: Linux x86 fault-context observation

## 결과 / Result

Linux x86 signal handler가 `siginfo_t`의 fault address/signal code와 i386 `ucontext_t`의 page-fault error code, 일반 레지스터를 보존하도록 확장했습니다. signal handler가 끝난 뒤 `NativeFaultObservation`이 mapped image와 guarded guest stack의 범위를 확인하고 bounded instruction/stack window를 복사합니다. 원본 자산은 저장하지 않습니다.

*Extended the Linux x86 signal handler to preserve fault address/signal code from `siginfo_t`, plus the page-fault error code and general registers from i386 `ucontext_t`. After the signal handler exits, `NativeFaultObservation` range-checks the mapped image and guarded guest stack, then copies bounded instruction/stack windows. No original assets are stored.*

실제 4th CHD의 GetVersion thunk 연속 실행은 다음 context를 출력했습니다.

*The real 4th CHD GetVersion thunk continuation printed this context:*

```text
GetVersion thunk not reached: signal 11, EIP 0x00af0c22
fault address   : 0x00000000, signal code=1, cpu error=0x00000004
fault registers : eax=00000000 ebx=ff8a4418 ecx=00000000 edx=00000000
                  esi=00ae04d0 edi=00af0cb8 ebp=f7797fbc eflags=00010246
fault code      : 0x00af0c1a 33c90f826ee9ffff8a08f7c546d883d581f9cc0000000f85
fault stack     : 7f000001 00af0d04 00af0cb8 00ae04d0
```

EIP 위치의 `8a 08`은 `MOV CL, byte ptr [EAX]`이고 EAX가 zero이므로 null read가 fault의 직접 원인으로 확인됩니다. error code `0x4`는 user-mode non-present page read와 일치합니다. EAX를 zero로 만든 producer는 아직 확인하지 않았습니다.

*The `8a 08` at EIP is `MOV CL, byte ptr [EAX]`; because EAX is zero, the null read is confirmed as the direct fault cause. Error code `0x4` matches a user-mode read from a non-present page. The producer that made EAX zero is not yet confirmed.*

## 검증 / Validation

WSL에서 Linux x86 product build와 synthetic probe를 실행했습니다. synthetic `UD2` fixture는 signal/EIP와 EIP 전후 24-byte instruction window, guarded stack의 하나 이상의 word를 검증합니다. 실제 4th CHD는 위 fault context를 반복 출력했습니다.

*Ran the Linux x86 product build and synthetic probe under WSL. The synthetic `UD2` fixture verifies signal/EIP, the 24-byte instruction window around EIP, and at least one guarded-stack word. The real 4th CHD repeatedly printed the fault context above.*

Linux x64 product build도 통과했고, i386 전용 diagnostic은 x64에서 명시적 오류로 거부됩니다.

*The Linux x64 product build also passed, and the i386-only diagnostic is explicitly rejected on x64.*
전용 `linux-x86-helper` preset의 `re2dj_linux_native_ipc_helper` 빌드도 통과했습니다. 따라서 확장된 `NativeGuestFault` 구조는 i386 product in-process runner와 i386 helper 모두에서 컴파일·링크됩니다.

*The dedicated `linux-x86-helper` preset also built `re2dj_linux_native_ipc_helper` successfully. The expanded `NativeGuestFault` structure therefore compiles and links in both the i386 product in-process runner and the i386 helper.*