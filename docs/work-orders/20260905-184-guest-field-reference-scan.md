# Task 184: 게스트 필드 참조 스캐너와 `+0xa10` 기록자 찾기

## 작업 목표

실행 중인 게스트 `.text`에서 주어진 32비트 상수를 참조하는 명령을 찾는 스캐너를 만들고, 그것으로 EZ2DJ 4th의 null 필드 `[0x00aca5b0 + 0xa10]`을 쓰는 코드가 어디에 있는지 찾습니다.

## 선행 문서

- [Task 184 설계](../design/20260905-184-guest-field-reference-scan.md)
- [Task 182 작업 로그](../work-logs/20260905-182-directx7-legacy-delegation.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)

## 구현 범위

1. **스캐너 추가.** `--field-reference-scan <hex>`를 추가합니다. 값은 32비트 상수이며 여러 번 지정할 수 있습니다. 첫 접근 위반 시점에 게스트 `.text`를 읽어 그 상수를 참조하는 명령을 찾습니다.

2. **두 참조 형태 처리.** `mod=10` 베이스+변위와 `mod=00, rm=101` 절대 주소를 모두 찾습니다. `rm=100`일 때 SIB 바이트로 변위 위치가 밀리는 경우를 포함합니다.

3. **접근 종류 분류.** opcode로 read / write / address / other를 구분합니다. 확실하지 않으면 `other`로 남기고 추측하지 않습니다.

4. **기록.** 명령마다 RVA, 절대 주소, 접근 종류, opcode, ModRM, 앞뒤 바이트 창을 남깁니다. 상한과 상한 도달 여부를 함께 남깁니다.

5. **자기 확인.** 변위 `0x11c` 스캔 결과가 기존 `null_context_field_reference` 기록과 같은 RVA 집합을 내는지 확인합니다.

6. **조사 실행.** `0xa10`과 절대 주소 `0x00acafc0`으로 스캔해 write 후보를 찾고, 결과를 작업 로그와 분석 문서에 남깁니다.

## 비범위

- 기존 `ScanRuntimeNullContextFieldReferences` 일반화 또는 제거.
- 디스어셈블러 도입.
- `+0xa10`이 가리키는 객체의 정체 확정.
- 찾은 기록자를 실제로 실행시키는 수정.

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
  --field-reference-scan 0xa10 --field-reference-scan 0x00acafc0 --field-reference-scan 0x11c
```

## 자기 검증 기준

- `0x11c` 스캔의 RVA 집합이 기존 기록과 일치해야 합니다. 다르면 스캐너가 틀린 것입니다.
- 기록된 각 후보의 바이트 창이 그 명령을 사람이 다시 해독할 수 있을 만큼 남아야 합니다.
- write 후보가 0건이면 그것을 결론으로 기록하고 추측을 덧붙이지 않습니다.

---

# Task 184: Guest Field Reference Scanner And The `+0xa10` Writer

## Goal

Build a scanner that finds instructions referencing a given 32-bit constant in the live guest's `.text`, and use it to locate the code that writes EZ2DJ 4th's null field `[0x00aca5b0 + 0xa10]`.

## Scope

Add `--field-reference-scan <hex>`, repeatable; match both base-plus-displacement and absolute-address forms including the SIB case; classify read, write, address, and other by opcode without guessing; record RVA, address, kind, opcode, ModRM, and a surrounding byte window under a cap; validate the scanner against the known `0x11c` displacement; then run the investigation and record what it finds.

## Out Of Scope

Generalizing or removing the existing scanner; adding a disassembler; identifying what the field points to; changing the guest's behavior.

## Verification

Build clean, run the unit tests and product loader probe, confirm the `0x11c` scan reproduces the known RVA set, then scan for `0xa10` and the absolute field address and record the result. Zero write candidates is a result, not a failure.
