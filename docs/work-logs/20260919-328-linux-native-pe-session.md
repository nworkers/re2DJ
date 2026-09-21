# 작업 로그 328: Linux native PE session 분리 / Work log 328: Linux native PE session extraction

## 결과 / Result

`NativePeSession`이 PE image, import thunk region, import gate table, guest bootstrap을 소유하도록 분리했습니다. helper IPC adapter는 session을 준비한 뒤 image/gate metadata를 protocol로 보고하고, TLS와 entry 실행을 session에 위임합니다.

*Extracted `NativePeSession` to own PE image, import thunk region, import gate table, and guest bootstrap. The helper IPC adapter prepares session, reports image/gate metadata through protocol, and delegates TLS and entry execution to session.*

이 단계는 in-process product backend가 아니며 helper IPC protocol과 actual target API completion을 바꾸지 않습니다.

*This step is not an in-process product backend and does not change helper IPC protocol or actual-target API completion.*

## 검증 / Validation

WSL Ubuntu 24.04 i386 helper build가 통과했습니다. Linux x64와 x86 host probe는 shared helper로 normal import `result=51 child=0`, SIGILL fault, terminal stop, capability rejection을 모두 통과했습니다.

*The WSL Ubuntu 24.04 i386 helper build passed. Linux x64 and x86 host probes both passed normal import `result=51 child=0`, SIGILL fault, terminal stop, and capability rejection using the shared helper.*
