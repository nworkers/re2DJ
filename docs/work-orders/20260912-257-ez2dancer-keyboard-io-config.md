# 작업 지시서: EZ2Dancer 키보드 I/O 설정
# Work Order: EZ2Dancer Keyboard I/O Configuration

## 한국어

설계 문서 [20260912-257](../design/20260912-257-ez2dancer-keyboard-io-config.md)에 따라
다음 작업을 수행합니다.

- [x] 공통 Win32 키 이름/INI binding parser를 분리합니다.
- [x] `Ez2DjKeyboardInput`을 공통 parser로 전환합니다.
- [x] `Ez2DancerKeyboardInput`을 추가합니다.
- [x] word-wide read 경로에 EZ2Dancer 키보드 poll을 연결합니다.
- [x] `config/ez2dancer-io.example.ini`를 추가합니다.
- [x] CLI 도움말과 README, ARCHITECTURE를 갱신합니다.
- [x] EZ2Dancer 키보드 설정 단위 테스트를 추가합니다.
- [x] Windows x86 Debug 빌드와 관련 테스트를 실행합니다.
- [x] 실제 `ez2d2m` 실행 로그를 확인합니다.
- [x] 분석 문서와 작업 로그를 갱신합니다.

코인 입력은 실제 port/bit가 확인되지 않았으므로 범위에서 제외합니다.

## English

Following [design 20260912-257](../design/20260912-257-ez2dancer-keyboard-io-config.md):

- [x] Extract the common Win32 key-name and INI binding parser.
- [x] Move `Ez2DjKeyboardInput` onto the common parser.
- [x] Add `Ez2DancerKeyboardInput`.
- [x] Connect EZ2Dancer keyboard polling to word-wide reads.
- [x] Add `config/ez2dancer-io.example.ini`.
- [x] Update CLI help, README, and ARCHITECTURE.
- [x] Add EZ2Dancer keyboard-config unit tests.
- [x] Build Windows x86 Debug and run the related tests.
- [x] Inspect a real `ez2d2m` run log.
- [x] Update the analysis and work log.

Coin input is out of scope because its actual port and bit remain unconfirmed.
