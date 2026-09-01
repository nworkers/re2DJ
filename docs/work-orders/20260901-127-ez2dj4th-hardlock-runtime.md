# ez2dj4th Hardlock runtime 작업 지시

관련 설계: [ez2dj4th Hardlock runtime](../design/20260901-127-ez2dj4th-hardlock-runtime.md)

*Related design: [ez2dj4th Hardlock runtime](../design/20260901-127-ez2dj4th-hardlock-runtime.md).*

## 범위

1. Task 126의 저장소 외부 전용 정책을 `cfg/` Git-ignore 기본 정책으로 갱신합니다.
2. 실제 키 설정을 `cfg/hardlock.ini`로 옮기고 기존 임시 설정 의존성을 제거합니다.
3. 제품 CLI와 launcher가 옵션 없이 profile별 기본 설정을 선택하도록 구현합니다.
4. 4th 원본 실행의 Hardlock device/IOCTL sequence를 bounded 방식으로 재확인합니다.
5. 근거가 확인된 protocol 처리와 Function `0x0e` transform을 플랫폼 중립 코드로 구현하고 runtime에 연결합니다.
6. 실제 CHD, Windows x86 build/test와 비밀값 비추적 검사를 수행합니다.

*Replace Task 126's outside-repository-only policy with a Git-ignored `cfg/` default, move the real configuration to `cfg/hardlock.ini`, make the product CLI and launcher select it per profile without an option, reacquire the original 4th Hardlock device/IOCTL sequence with bounded diagnostics, implement policy-compatible protocol and Function `0x0e` behavior in platform-neutral code, and run real-CHD, Windows x86, and secret non-tracking verification.*

## 완료 조건

- `cfg/hardlock.ini`가 존재하지만 Git status와 commit 대상에는 나타나지 않습니다.
- 4th는 옵션 없이 해당 profile section을 읽고 다른 profile key를 사용하지 않습니다.
- 키값·변환 block이 source, test, documentation, command line과 product log에 없습니다.
- 구현한 Hardlock 응답이 실제 원본의 다음 분기를 진행시키거나, 미확정 근거와 정확한 다음 blocker가 기록됩니다.

*Completion requires an existing but untracked `cfg/hardlock.ini`; option-free loading of only the 4th profile section; no keys or transform blocks in source, tests, documentation, command lines, or product logs; and either progression of the original execution's next branch or a precise record of the remaining evidence blocker.*
