# Task 186: 게스트 코드 창 기록과 설치 경로 추적

## 작업 목표

실행 중인 게스트의 임의 주소에서 바이트 창을 읽는 진단을 만들고, 그것으로 EZ2DJ 4th의 생성자 호출자와 결함 호출 사슬을 읽어 `+0xa10` 객체를 만들었어야 하는 코드를 찾습니다.

## 선행 문서

- [Task 186 설계](../design/20260905-186-guest-code-window.md)
- [Task 185 작업 로그](../work-logs/20260905-185-field-write-watch.md)
- [Task 184 작업 로그](../work-logs/20260905-184-guest-field-reference-scan.md)

## 구현 범위

1. **옵션 추가.** `--code-window <hex-address>[:<hex-length>]`. 반복 지정 가능. 길이를 생략하면 128, 상한 512입니다.

2. **기준점 방식.** 지정 주소를 창의 시작이 아니라 기준점으로 삼아 앞 절반과 뒤 절반을 읽습니다. 복귀 주소를 그대로 지정해도 그 앞의 `call`이 창에 들어와야 합니다.

3. **기록.** 주소, RVA, 섹션, 창 시작 주소, 길이, 읽기 성공 여부, 바이트 열을 남깁니다. 읽지 못하면 실패를 명시합니다.

4. **실행 시점.** 첫 접근 위반에서 한 번 읽습니다. Task 184의 스캔과 같은 시점입니다.

5. **조사 실행과 해독.** 아래 다섯 지점을 한 실행에서 읽고 손으로 해독해 작업 로그에 남깁니다.

   | 주소 | 무엇 |
   | --- | --- |
   | `0x004a5c26` | 생성자의 호출자 |
   | `0x004235fa` | 결함 함수의 호출자 |
   | `0x0043627e` | 그 위 호출자 |
   | `0x004076ef` | 그 위 호출자 |
   | `0x00422a50` | null 검사를 가진 형제 함수 |

6. **문서 갱신.** 해독 결과를 작업 로그와 `docs/analysis/ez2dj4th-graphics-path.md`에 반영합니다. 바이트 열 전체는 옮기지 않고 해독과 오프셋만 남깁니다.

## 비범위

- 디스어셈블러 도입.
- 복호화된 `.text` 전체 덤프.
- 기존 바이트 창 기록들의 통합.
- 게스트 동작 변경.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common `
  --code-window 004225db `
  --code-window 004a5c26 `
  --code-window 004235fa `
  --code-window 0043627e `
  --code-window 004076ef `
  --code-window 00422a50:100
```

## 자기 검증 기준

- `0x004225db` 창 안에 `c780100a0000 00000000`이 있어야 합니다. 없으면 창 계산이 틀린 것입니다.
- 각 창의 시작 주소가 지정 주소보다 앞이어야 합니다.
- 해독을 작업 로그에 적을 때 근거 바이트를 함께 남겨 재검증이 가능해야 합니다.

---

# Task 186: Guest Code Window Capture And Tracing The Install Path

## Goal

Add a diagnostic that reads a byte window at any address in the live guest, and use it to read EZ2DJ 4th's constructor caller and faulting call chain to find the code that should have created the `+0xa10` object.

## Scope

Add a repeatable `--code-window <hex-address>[:<hex-length>]` that treats the address as a midpoint so a return address brings its own `call` into view, records the window with its section and read status, captures at the first access violation, and then read and hand-decode the five sites above.

## Out Of Scope

A disassembler, a full dump of decrypted `.text`, consolidating the existing byte-window records, and changing guest behavior.

## Verification

Build clean, run the unit tests and product loader probe, confirm the known store bytes appear in the window for `0x004225db`, and record each decoding together with the bytes it rests on.
