# ez2dj3rd Hardlock descriptor key material capture work order

상태: 완료 — synthetic 검증 완료, 원본 자산 부재를 잔여 외부 입력으로 기록

*Status: complete — synthetic verification passed; missing original assets are recorded as the remaining external input.*

Related design: [ez2dj3rd Hardlock descriptor key material capture](../design/20260831-106-ez2dj3rd-hardlock-descriptor-key-material.md)

## 목표

3rd Hardlock descriptor의 `ID_Ref`/`ID_Verify`를 전체 packet dump 없이 재현 가능하게 수집하여 오프라인 seed 복구의 다음 입력을 확보합니다.

*Capture the 3rd Hardlock descriptor's `ID_Ref`/`ID_Verify` reproducibly without a complete packet dump, providing the next input for offline seed recovery.*

## 작업

1. 플랫폼 중립 32-bit `HL_API` fixed-header parser와 단위 테스트를 추가합니다.
2. `0x9c40244c`/`0x9c402458` synthetic boundary에 bounded descriptor marker를 추가합니다.
3. 전용 Windows runtime probe로 marker 범위와 비변경 IOCTL semantics를 검증합니다.
4. Hardlock descriptor 구조와 외부 출처를 KB에 반영합니다.
5. Windows x86 Debug build, CTest, `git diff --check`를 수행합니다.
6. 3rd 원본 자산이 있으면 bounded 실행 두 번으로 실제 값을 확인하고, 없으면 필요한 사용자 입력과 미확정 상태를 작업 로그에 남깁니다.
7. analysis, architecture, TODO/IMPLEMENTED와 작업 로그를 실제 결과에 맞춰 갱신하고 커밋합니다.

*Add and test a platform-neutral packed-32-bit `HL_API` fixed-header parser; add a bounded descriptor marker at synthetic `0x9c40244c`/`0x9c402458`; verify marker scope and unchanged IOCTL semantics in a dedicated Windows runtime probe; update the knowledge base with public structure references; run the Windows x86 Debug build, CTest, and `git diff --check`; perform two bounded original runs when the 3rd assets are available or record the missing external input; then update cumulative documents and commit.*

## 완료 조건

- synthetic descriptor의 fixed fields가 정확한 offset으로 해석됩니다.
- runtime marker는 전체 packet이나 reserved bytes를 기록하지 않습니다.
- 기존 synthetic response 동작과 1st SE LPTDI 경로가 바뀌지 않습니다.
- 실제 `ID_Ref`/`ID_Verify`를 확인했거나, 원본 자산 부재 때문에 남은 검증을 명시적으로 기록합니다.

*Completion requires correct fixed-field offsets, no complete-packet or reserved-byte logging, no change to existing synthetic responses or the 1st SE LPTDI path, and either confirmed actual `ID_Ref`/`ID_Verify` values or an explicit record that original assets are unavailable for the remaining verification.*
