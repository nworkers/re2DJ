# 작업 로그 325: 단일 프로세스 x86 실행 backend 설계 / Work log 325: Single-process x86 execution-backend design

## 결과 / Result

Linux는 x86 guest와 HLE를 한 runtime process에 두는 방향으로 변경했습니다. Linux x86은 native i386 process, Linux x64는 x86 compatibility-mode와 64비트 HLE trampoline을 사용하는 backend를 목표로 합니다. 현재 i386 helper IPC backend는 검증 전환 동안 diagnostic fallback으로 유지합니다.

*Linux now targets one runtime process containing x86 guest and HLE. Linux x86 targets a native i386 process; Linux x64 targets a backend with x86 compatibility mode and 64-bit HLE trampolines. The current i386-helper IPC backend remains a diagnostic fallback during validation transition.*

Windows x64는 같은 목표를 OS WoW64가 호스팅한 i386 runtime process로 달성합니다. 64비트 Windows process 안에 32비트 runtime을 직접 load하는 비지원 구조는 채택하지 않습니다. 이 작업은 설계만 변경하며 runtime code나 Windows behavior를 변경하지 않았습니다.

*Windows x64 achieves the same goal with an i386 runtime process hosted by OS WoW64. It does not adopt the unsupported structure of directly loading 32-bit runtime into a 64-bit Windows process. This task changes design only; it changes neither runtime code nor Windows behavior.*
