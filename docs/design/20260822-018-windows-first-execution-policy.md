# Windows-First Execution Policy

## 한국어

### 결정

Stage 4부터 Stage 7까지의 기능 개발·통합 검증은 64비트 Windows와 별도 Win32 x86 helper를 기준으로 진행합니다. Linux와 Web은 공용 인터페이스가 바뀌지 않는 범위의 빌드 유지와 이미 완료된 helper probe만 수행하며, Windows에서 원본 `ez2dj1.exe`가 해당 단계 완료 기준에 도달한 뒤 확장합니다.

### 이유

원본이 Win32 실행 파일이므로 Windows에서 API 의미와 경로·창·DirectX 동작을 가장 적은 변환 계층으로 관찰할 수 있습니다. 먼저 Windows에서 실제 import 호출 순서를 확인하면 Linux/Web backend가 추측에 의존하는 것을 막을 수 있습니다.

### 다음 Windows 작업

첫 작업은 원본 PE32를 Windows helper에 적재하고, 구현되지 않은 import gate의 module/name/ordinal과 guest call frame을 구조적으로 기록하는 dispatcher입니다. 이 작업은 게임 로직을 재구현하지 않으며, Stage 4의 kernel32/user32 HLE 범위를 실행 관찰로 좁힙니다.

## English

### Decision

From Stage 4 through Stage 7, feature development and integration verification use 64-bit Windows with the separate Win32 x86 helper as the reference path. Linux and Web retain build health and completed helper probes only, until Windows reaches each stage's original-`ez2dj1.exe` completion criterion.

### Rationale

The original is a Win32 executable, so Windows observes API semantics, paths, windows, and DirectX behavior through the fewest translation layers. Confirming real import-call order on Windows prevents Linux/Web backends from depending on guesses.

### Next Windows work

The first task loads the original PE32 in the Windows helper and structurally records module/name/ordinal plus guest call-frame information for unimplemented import gates. It does not reimplement gameplay; it narrows Stage 4 kernel32/user32 HLE scope through execution observation.
