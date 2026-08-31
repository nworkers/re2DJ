# ez2dj3rd Hardlock 0x9c40244c descriptor 응답 경계 작업 지시

관련 설계: [ez2dj3rd Hardlock 0x9c40244c descriptor 응답 경계](../design/20260901-110-ez2dj3rd-hardlock-44c-response.md)

*Related design: [ez2dj3rd Hardlock `0x9c40244c` descriptor response boundary](../design/20260901-110-ez2dj3rd-hardlock-44c-response.md).*

## 범위

1. 현재·과거 Function 0 descriptor의 bounded fixed/tail fields를 비교합니다.
2. descriptor parser와 runtime marker에 exact 256-byte tail word를 추가합니다.
3. 전용 probe와 단위 테스트를 보강합니다.
4. `0x450` replay를 유지한 bounded 원본 실행에서 `0x44c` post-IOCTL trace를 수집합니다.
5. user-supplied 16-bit tail patch를 exact Function 0 `0x44c`에만 적용하는 기본 비활성 분석 옵션으로 구현하고 tail 1 분기를 검증합니다.
6. 반환 뒤 Status/Function/tail 소비와 첫 상위 분기를 확인·추정·미확정으로 문서화합니다.
7. Windows x86 Debug build, CTest와 `git diff --check`를 수행합니다.
8. 작업 단위 변경을 커밋합니다.

*Compare bounded fixed/tail fields from current and historical Function-0 descriptors; extend the parser, runtime marker, dedicated probe, and unit tests with the exact 256-byte tail word; collect a bounded `0x44c` post-IOCTL trace while retaining `0x450` replay; document the post-return consumer and first upper branch with explicit certainty; run Windows x86 Debug build, CTest, and `git diff --check`; and commit the task unit.*

## 완료 조건

- 전체 descriptor나 원본 asset을 새로 저장하지 않습니다.
- tail 진단은 exact 256-byte `0x44c/458` descriptor에만 적용됩니다.
- synthetic buffer 보존을 실제 driver 응답으로 기술하지 않습니다.
- 실행 child는 제한 시간 뒤 경로와 종료 상태를 확인하며, 남아 있을 때만 정확한 PID를 종료합니다.

*No complete descriptor or original asset is newly stored; tail diagnostics apply only to exact 256-byte `0x44c/458` descriptors; synthetic buffer preservation is not described as a physical-driver response; and the child path and exit state are checked after the bounded run, terminating only the exact PID if it remains alive.*
