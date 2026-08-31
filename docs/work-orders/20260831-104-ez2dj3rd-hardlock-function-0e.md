# ez2dj3rd Hardlock Function 0x0E 분석 작업 지시

## 목표

3rd 원본의 실제 Hardlock device/API 경계를 확인하고, 1st SE LPTDI 응답을 잘못 적용하지 않으면서 다음 구현에 필요한 seed 또는 Function `0x0e` 응답 입력을 특정합니다.

*Goal: confirm the actual Hardlock device/API boundary of the 3rd original, identify the seed or Function `0x0e` response input needed for the next implementation, and avoid incorrectly applying the 1st SE LPTDI response.*

## 작업 항목

1. all-slot `GetProcAddress` IAT 연결을 빌드하고 3rd 실행에 적용합니다.
2. 동적 `CreateFileA` 요청의 실제 장치 이름과 후속 IOCTL sequence를 기록합니다.
3. 256바이트 Hardlock descriptor와 264바이트 Function `0x0e` packet을 분석합니다.
4. 1st SE의 `0x9c406410/414`와 3rd의 `0x9c402450/44c/458`가 서로 다른 계약임을 문서화합니다.
5. 정확한 0x0e 응답을 확인하지 못하면 성공으로 표시하지 않고, 필요한 외부 증거와 재현 가능한 다음 경계를 기록합니다.

*Tasks: build and apply the all-slot `GetProcAddress` IAT routing; record the actual dynamic `CreateFileA` device names and subsequent IOCTL sequence; analyze the 256-byte Hardlock descriptor and 264-byte Function `0x0e` packet; document that 1st SE's `0x9c406410/414` and 3rd's `0x9c402450/44c/458` are different contracts; and, if the exact 0x0e response is not confirmed, do not mark success and record the required external evidence and reproducible next boundary.*

## 완료 기준

- 동일 이름의 3rd `GetProcAddress` IAT 슬롯 두 개가 모두 runtime resolver로 연결됩니다.
- 프로파일이 `\\.\\FEnteDev`를 사용하고 `NTICE`는 자동 성공시키지 않습니다.
- Windows x86 build와 CTest가 통과합니다.
- 분석 문서와 작업 로그에 0x0e 응답의 미확정 상태와 다음 입력 조건이 남습니다.

*Completion criteria: both matching 3rd `GetProcAddress` IAT slots route to the runtime resolver; the profile uses `\\.\\FEnteDev` without automatically succeeding `NTICE`; the Windows x86 build and CTest pass; and the analysis and work log record the unresolved 0x0e response and the conditions for the next input.*
