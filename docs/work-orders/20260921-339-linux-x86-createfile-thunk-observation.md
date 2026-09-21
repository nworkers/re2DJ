# 작업 지시 339: Linux x86 CreateFileA 동적 thunk 관측 / Work order 339: Linux x86 dynamic CreateFileA thunk observation

4th Trax의 확인된 `GetProcAddress(kernel32, "CreateFileA")` 요청에 process-local executable thunk를 반환하고, 원본 코드의 실제 호출과 7개 stdcall 인자를 관측합니다. 파일·장치 동작을 추정하지 않고 실패 handle로 복귀하여 호출 ABI 경계만 검증합니다.

*Return a process-local executable thunk for 4th Trax's confirmed `GetProcAddress(kernel32, "CreateFileA")` request and observe the original code's actual call plus seven stdcall arguments. Do not infer file or device behavior; return a failure handle and validate only the call ABI boundary.*

완료 기준은 다음과 같습니다.

*Completion criteria:*

1. Linux i386 진단이 GetVersion과 CreateFileA resolver 결과에 서로 다른 executable thunk를 제공합니다.
   *The Linux i386 diagnostic provides distinct executable thunks for the GetVersion and CreateFileA resolver results.*
2. CreateFileA dynamic gate가 복귀 주소, bounded 파일 이름, 6개 scalar 인자를 구조화해 보존합니다.
   *The CreateFileA dynamic gate structurally preserves the return address, bounded file name, and six scalar arguments.*
3. synthetic probe와 실제 4th CHD 실행에서 thunk 호출과 stdcall 복귀를 검증합니다.
   *Validate thunk invocation and stdcall return with the synthetic probe and a real 4th CHD run.*
4. Linux x86/x64 빌드, 관련 설계·분석·아키텍처 갱신, 작업 로그, Git 커밋을 남깁니다.
   *Leave Linux x86/x64 builds, related design/analysis/architecture updates, a work log, and a Git commit.*
