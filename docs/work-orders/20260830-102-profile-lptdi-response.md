# 프로파일별 Hardlock/LPTDI 응답 작업 지시

## 목적

1st SE의 LPTDI challenge-response 구현을 3rd Hardlock에 재사용하되, 장치 이름과 콘텐츠별 seed/target state를 프로파일마다 독립적으로 전달하고 검증한다.

*Reuse the 1st SE LPTDI challenge-response implementation for 3rd Hardlock while independently carrying and validating the device name and content-specific seed/target state per profile.*

## 작업 범위

1. 관련 설계 문서를 작성한다.
2. `TargetLptdiPolicy`에 프로파일별 synthetic device path prefix를 추가한다.
3. launcher와 injected runtime이 선택된 프로파일의 path prefix와 target state를 사용하도록 수정한다.
4. 1st SE와 3rd의 profile defaults, product loader probe, VFS runtime probe를 갱신한다.
5. 3rd 실행에서 Hardlock 이후 경계를 확인하고, 확인되지 않은 응답 payload는 추측하지 않는다.
6. 분석 문서와 작업 로그를 갱신하고 Windows x86 검증을 수행한다.

*Scope: write the design; add a per-profile synthetic-device path prefix; make the launcher and injected runtime use the selected profile's prefix and target state; update 1st/3rd defaults and probes; run 3rd through the post-Hardlock boundary without guessing an unconfirmed payload; update analysis and work-log documents; and verify Windows x86.*

## 완료 기준

- 1st SE와 3rd가 같은 challenge-response 변환을 공유한다.
- 1st SE는 `\\.\\LPTDI`, 3rd는 `\\.\\Hardlock`을 사용한다.
- profile state가 서로 독립적이며 1st SE의 target state가 3rd에 자동 복사되지 않는다.
- command-line override가 있으면 profile default보다 우선한다.
- unit test, VFS runtime probe, Windows product loader probe와 build가 통과한다.
- 3rd 실제 실행 결과와 미확정 항목이 문서화된다.

*Completion criteria: 1st SE and 3rd share the same challenge-response transform; 1st SE uses `\\.\\LPTDI` and 3rd uses `\\.\\Hardlock`; profile states remain independent and 1st SE's target state is never copied implicitly; command-line overrides take precedence; unit, VFS runtime, product-loader, and build verification pass; and the 3rd execution result and unresolved items are documented.*

## 진행 결과

프로파일별 path prefix와 target state 전달, 동적 `GetProcAddress` wrapper, 3rd Hardlock mock을 구현했다. 3rd의 `0000000000000000`은 1st SE 값과 분리된 zero-state probe로만 기록하며 유효한 Hardlock seed로 확정하지 않는다. Windows x86 빌드·CTest와 실제 `re2dj ez2dj3rd` bounded 실행을 완료했고, 동적 보호 요청이 VFS trace를 생성하지 않은 점을 미확정 항목으로 남긴다.

*Implemented profile-specific path-prefix and target-state propagation, the dynamic `GetProcAddress` wrapper, and the 3rd Hardlock mock. The 3rd `0000000000000000` value is recorded only as a zero-state probe separate from 1st SE, not as a confirmed valid Hardlock seed. Windows x86 build/CTest and a bounded real `re2dj ez2dj3rd` run completed; the absence of a VFS trace leaves the dynamic protection request unresolved.*
