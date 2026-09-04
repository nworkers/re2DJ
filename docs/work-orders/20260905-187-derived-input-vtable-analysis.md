# Task 187: 파생 입력 vtable 분석과 클래스 계통 식별

## 작업 목표

기저 vtable `0x004dd16c`과 파생 vtable `0x004e20a0`의 슬롯들을 덤프 및 해독하여 결함 객체 클래스의 가상 메서드 구성을 확정하고, 인접한 형제 생성자군(`0x004a5be0` ~ `0x004a5cb0`) 및 전역 객체 `0x00aca5b0`의 참조자를 분석하여 다른 입력 파생 클래스 존재 여부와 객체 생성 흐름을 규명합니다.

## 선행 문서

- [Task 187 설계](../design/20260905-187-derived-input-vtable-analysis.md)
- [Task 186 작업 로그](../work-logs/20260905-186-guest-code-window.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)

## 구현 및 실행 범위

1. **진단 실행**:
   `re2dj_windows_x86_launcher_probe`를 실행하여 다음 정보를 수집합니다:
   - 파생 vtable 메모리 창: `--code-window 004e20a0:200`
   - 기저 vtable 메모리 창: `--code-window 004dd16c:200`
   - 선행 생성자 메모리 창: `--code-window 004a5be0:100`
   - 후행 생성자 메모리 창: `--code-window 004a5c60:100`
   - 전역 객체 참조자 스캔: `--field-reference-scan 00aca5b0`
   - 파생 vtable 참조자 스캔: `--field-reference-scan 004e20a0`

2. **vtable 슬롯 해독 및 비교**:
   - `0x004dd16c` 및 `0x004e20a0`의 각 4바이트 슬롯 함수 주소를 표로 정리.
   - 기저 클래스와 파생 클래스의 오버라이드 현황 비교.
   - 결함 함수 `0x00422b00`이 가상 메서드 슬롯에 포함되어 있는지 확인.

3. **형제 생성자군 해독**:
   - `0x004a5be0` 및 `0x004a5c60` 창 내의 함수들을 Capstone 5.0.7로 역어셈블하여 기저 클래스 호출 여부 및 설치하는 vtable 주소 규명.

4. **전역 객체 참조자 분석**:
   - `0x00aca5b0` 참조 지점들을 분류(초기화 호출, 갱신 루프 호출, 상태 전달 등).

5. **결과 문서화**:
   - 작업 로그(`docs/work-logs/20260905-187-derived-input-vtable-analysis.md`) 및 분석 문서(`docs/analysis/ez2dj4th-graphics-path.md`) 갱신.

## 비범위

- 게스트 코드 패치 또는 런타임 동작 수정.
- DirectInput HLE 인터페이스 구현 (이는 vtable 분석 완료 후 후속 작업).
- 저장소 내 제품 바이너리에 디스어셈블러 통합.

## 최소 검증

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common `
  --field-reference-scan 00aca5b0 `
  --field-reference-scan 004e20a0 `
  --code-window 004e20a0:200 `
  --code-window 004dd16c:200 `
  --code-window 004a5be0:100 `
  --code-window 004a5c60:100
```

## 자기 검증 기준

- `0x004dd16c` 및 `0x004e20a0`의 슬롯 0 주소가 유효한 `.text` 섹션 범위(`0x00401000` ~ `0x004dc000`)에 있어야 합니다.
- `0x004a5c10`의 vtable 대입 명령(`c700a0204e00` -> `mov [eax], 0x4e20a0`)과 일치하는 슬롯 주소들이 확인되어야 합니다.
- 모든 해독 결과는 근거 바이트와 함께 작업 로그에 기록되어야 합니다.

---

# Task 187: Derived Input Vtable Analysis And Class Hierarchy Identification

## Goal

Dump and decode the slots of base vtable `0x004dd16c` and derived vtable `0x004e20a0` to confirm the virtual method layout of the faulting class, analyze sibling constructors (`0x004a5be0` to `0x004a5cb0`), and scan references to global object `0x00aca5b0` to determine whether alternative input classes exist and how the object is instantiated.

## Scope

1. Run launcher probe with `--code-window` for the two vtables and sibling constructors, and `--field-reference-scan` for `0x00aca5b0` and `0x004e20a0`.
2. Compare and tabulate virtual method slots between base and derived classes.
3. Decode sibling constructors with Capstone 5.0.7 to identify alternative vtables.
4. Categorize references to global object `0x00aca5b0`.
5. Update work log and analysis documentation.

## Out Of Scope

Modifying guest code or runtime behavior, implementing DirectInput HLE, or adding disassemblers to production code.

## Verification

Run launcher probe diagnostic, verify slot pointers fall within `.text`, confirm consistency with constructor stores, and record decodings with raw bytes in the work log.
