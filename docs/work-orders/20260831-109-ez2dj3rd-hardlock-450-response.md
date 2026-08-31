# ez2dj3rd Hardlock 0x9c402450 응답 경계 작업 지시

관련 설계: [ez2dj3rd Hardlock 0x9c402450 응답 경계](../design/20260831-109-ez2dj3rd-hardlock-450-response.md)

*Related design: [ez2dj3rd Hardlock `0x9c402450` response boundary](../design/20260831-109-ez2dj3rd-hardlock-450-response.md).*

## 범위

1. 과거 `0x450` packet과 caller bytes를 주소·buffer layout 기준으로 재검증합니다.
2. Windows injected runtime에 exact-size, control-code 전용 bounded packet 진단을 추가합니다.
3. 전용 Hardlock runtime probe에 marker와 비대상 제외 검증을 추가합니다.
4. active-session HLE에서 zero-byte/full-size 원본 실행을 bounded 비교합니다.
5. 기존 post-IOCTL trace가 보호 초기화를 교란하지 않으면 `0x450` 반환 소비와 상위 분기를 수집합니다.
6. 별도 parser/state로 user-supplied exact 6-byte `0x450` replay 옵션을 구현하고 synthetic oracle을 bounded 검증합니다.
7. 확인·추정·미확정을 analysis, architecture, TODO와 작업 로그에 반영합니다.
8. 영향 대상 Windows x86 Debug build와 CTest, `git diff --check`를 수행합니다.
9. 작업 단위 변경을 커밋합니다.

*Revalidate historical packets and caller bytes; add an exact-size, control-code-specific bounded packet diagnostic plus dedicated-probe coverage; compare bounded active-session original runs under zero-byte and full-size policies; capture post-`0x450` consumption; implement a user-supplied exact six-byte replay option with separate parser/state and test the synthetic oracle; update cumulative documentation with explicit confirmation status; run the affected Windows x86 Debug build, CTest, and `git diff --check`; and commit the task unit.*

## 완료 조건

- 기록된 6바이트가 동일 실행의 request 순서와 연결됩니다.
- 진단은 응답을 수정하거나 synthetic 결과를 실제 driver payload로 간주하지 않습니다.
- `0x44c/458` 도달 여부를 관찰보다 강하게 기술하지 않습니다.
- 원본 파일·overlay와 다른 프로세스를 변경하지 않습니다.

*The six-byte values are tied to request order in the same run; diagnostics neither modify responses nor treat synthetic results as physical-driver payloads; later-call reachability is not overstated; and neither original files, overlay, nor unrelated processes are modified.*
