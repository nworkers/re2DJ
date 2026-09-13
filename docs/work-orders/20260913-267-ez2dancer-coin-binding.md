# 작업 지시서: EZ2Dancer coin 입력 바인딩
# Work Order: EZ2Dancer Coin Input Binding

## 한국어

설계 문서 [20260913-267](../design/20260913-267-ez2dancer-coin-binding.md)에 따라
`ez2d2m`용 EZ2Dancer keyboard I/O 설정에 coin 입력을 추가합니다.

- [x] `Ez2DancerButton`에 coin 상태를 추가합니다.
- [x] `0x304`의 추정 호환 coin bit를 board read에 연결합니다.
- [x] `Ez2DancerKeyboardInput`에 `coin` binding을 연결합니다.
- [x] `config/ez2dancer-io.example.ini`에 `coin=F5`를 추가합니다.
- [x] board 및 keyboard adapter 단위 테스트를 갱신합니다.
- [x] README, ARCHITECTURE, EZ2Dancer 분석 문서에 확인 상태를 반영합니다.
- [x] Windows x86 Debug 빌드와 관련 CTest를 실행합니다.
- [x] 작업 로그를 남기고 변경을 커밋합니다.

실제 원본의 coin port/bit는 미확정으로 유지하며, 이를 확정된 사실처럼 문서화하지
않습니다.

## English

Following [design 20260913-267](../design/20260913-267-ez2dancer-coin-binding.md), add
coin input to the EZ2Dancer keyboard I/O configuration used by `ez2d2m`.

- [x] Add coin state to `Ez2DancerButton`.
- [x] Connect the inferred compatibility coin bit on `0x304` to board reads.
- [x] Add the `coin` binding to `Ez2DancerKeyboardInput`.
- [x] Add `coin=F5` to `config/ez2dancer-io.example.ini`.
- [x] Update board and keyboard-adapter unit tests.
- [x] Reflect confirmation status in README, ARCHITECTURE, and the EZ2Dancer analysis.
- [x] Run the Windows x86 Debug build and related CTest tests.
- [x] Leave a work log and commit the change.

The original coin port and bit remain unresolved and must not be documented as confirmed.
