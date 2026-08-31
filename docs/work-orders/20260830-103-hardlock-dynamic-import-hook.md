# 3rd Hardlock 동적 import 후킹 작업 지시

## 목표

3rd 보호 진입 stub가 사용하는 두 번째 `KERNEL32.dll!GetProcAddress` 슬롯까지 injected runtime으로 연결해, Hardlock 동적 API 요청을 실제 HLE trace로 관찰할 수 있게 한다.

*Goal: route the second `KERNEL32.dll!GetProcAddress` slot used by the 3rd protected entry stub to the injected runtime so that the dynamic Hardlock API request can be observed through the HLE trace.*

## 작업 범위

1. PE IAT 이름 검색에 all-match API를 추가한다.
2. launcher의 dynamic resolver 설치를 all-match API로 변경한다.
3. 기존 1st SE 단일 슬롯 호출과 정적 VFS import 후킹의 회귀 여부를 확인한다.
4. 복호화 payload 문자열 및 3rd 실제 실행 trace로 Hardlock API 경계를 재확인한다.
5. 유효한 seed/응답값은 관찰 근거 없이 추정하지 않는다.
6. 관련 analysis와 작업 로그를 갱신하고 Windows x86 검증을 수행한다.

*Scope: add the all-match PE IAT query; change dynamic resolver installation to use it; check regressions in existing 1st SE single-slot calls and static VFS hooks; reconfirm the Hardlock API boundary using decrypted-payload strings and a real 3rd trace; avoid guessing a valid seed/response; update analysis and work-log documents; and run Windows x86 verification.*

## 완료 기준

- 3rd의 모든 `GetProcAddress` IAT 슬롯에 HLE resolver가 설치된다.
- build와 CTest가 통과한다.
- `re2dj ez2dj3rd` 실행에서 동적 Hardlock 요청이 trace에 나타나거나, 후속 실패 경계가 재현 가능하게 기록된다.
- 유효 응답 payload가 아직 미확정이면 그 상태가 문서에 명시된다.

*Completion criteria: all 3rd `GetProcAddress` IAT slots receive the HLE resolver; build and CTest pass; `re2dj ez2dj3rd` produces a dynamic Hardlock trace or a reproducible subsequent failure boundary; and the documentation explicitly states if the valid response payload remains unresolved.*
