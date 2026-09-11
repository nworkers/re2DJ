# EZ2Dancer 키보드 I/O 설정 작업 로그
# Work Log: EZ2Dancer Keyboard I/O Configuration

## 한국어

### 결과

`config/ez2dancer-io.example.ini`를 추가하고 `ez2d2m --io-config <path>`가 실제 word-wide I/O board에 키 상태를 전달하도록 연결했습니다. 기존 EZ2DJ adapter의 key parser를 공통 모듈로 분리했으며, EZ2DJ 동작은 유지했습니다.

예제는 양쪽 floor pad 6개, hand sensor 8개, TEST와 SERVICE를 제공합니다. 실제 port와 bit가 확인되지 않은 coin은 추가하지 않았습니다.

### 검증

- Windows x86 Debug 전체 빌드 성공: `cmd /c scripts\build_win32.bat`
- 관련 CTest 4개 성공: EZ2DJ keyboard, EZ2Dancer keyboard, product loader, core unit tests
- 실제 실행: `.\build\windows-x86\bin\Debug\re2dj.exe ez2d2m --io-config .\config\ez2dancer-io.example.ini`
- 실행 로그: `logs/windows_x86_launcher_probe/ez2d2m/20260912-034218-362.vfs.log`
- 관찰: `0x300`, `0x302`, `0x304`, `0x306`의 16비트 read가 모두 `handled=1`이었고 설정 오류는 기록되지 않았습니다.

물리 키를 눌렀을 때 게임 화면에서 반응하는지는 자동 검증하지 않았습니다. 현재 adapter의 button 이름과 bit 배치는 기존 분석에서 **추정**으로 분류된 계약을 따릅니다.

## English

### Result

Added `config/ez2dancer-io.example.ini` and connected `ez2d2m --io-config <path>` to the word-wide I/O board. Key-name parsing was extracted from the existing EZ2DJ adapter into a shared module without changing its behavior.

The example exposes six floor-pad zones, eight hand sensors, TEST, and SERVICE. Coin remains omitted because its real port and bit are unconfirmed.

### Verification

- Full Windows x86 Debug build passed: `cmd /c scripts\build_win32.bat`
- Four related CTest targets passed: EZ2DJ keyboard, EZ2Dancer keyboard, product loader, and core unit tests
- Real run: `.\build\windows-x86\bin\Debug\re2dj.exe ez2d2m --io-config .\config\ez2dancer-io.example.ini`
- Runtime log: `logs/windows_x86_launcher_probe/ez2d2m/20260912-034218-362.vfs.log`
- Observation: 16-bit reads from `0x300`, `0x302`, `0x304`, and `0x306` were all `handled=1`, with no configuration error recorded.

Physical key response in the game UI was not automated. Button names and bit assignments follow the contract classified as **inferred** by the existing analysis.
