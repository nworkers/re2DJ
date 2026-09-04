# Task 185: 필드 쓰기 감시점으로 `+0xa10`의 쓰기 관측

## 작업 목표

게스트의 임의 주소에 하드웨어 쓰기 감시점을 거는 진단을 만들고, 그것으로 EZ2DJ 4th의 결함 필드 `0x00acafc0`에 일어나는 모든 쓰기를 관측합니다. Task 184가 남긴 세 가설 중 어느 것인지 가립니다.

## 선행 문서

- [Task 185 설계](../design/20260905-185-field-write-watch.md)
- [Task 184 작업 로그](../work-logs/20260905-184-guest-field-reference-scan.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)

## 구현 범위

1. **옵션 추가.** `--field-write-watch <hex-address>`. 게스트 절대 주소를 받고 4바이트 정렬을 요구합니다. 정렬되지 않으면 거절합니다.

2. **감시점 무장.** `Dr1`에 4바이트 쓰기 감시점을 설정합니다. 프로세스의 첫 스레드와 이후 생성되는 모든 스레드에 무장합니다.

3. **조합 거절.** `Dr1`을 쓰는 `--slot-writer-trace`, `--null-context-field-reference-execution-trace`와 함께 지정하면 사용법을 출력하고 종료합니다.

4. **적중 기록.** 순번, 스레드, `EIP`와 RVA와 섹션, 감시 주소의 현재 값, 범용 레지스터, `[ebp+4]` 복귀 주소와 RVA, `EIP` 앞뒤 바이트 창을 남깁니다. 상한과 상한 도달 여부를 함께 남깁니다.

5. **조사 실행.** `0x00acafc0`을 감시하며 4th를 실행하고, 적중 지점·값·순서를 결함 시점과 대조해 작업 로그와 분석 문서에 남깁니다.

## 비범위

- 기존 `SetNullContextFieldAccessBreakpoint` 계열 일반화 또는 제거.
- 읽기 감시나 실행 중단점.
- 여러 주소 동시 감시.
- 찾은 설치 경로를 실제로 실행시키는 수정.

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
  --field-write-watch 00acafc0
```

## 자기 검증 기준

- Task 184가 찾은 `0x000225d1`과 `0x00022aad`가 적중 기록에 나타나야 합니다. 없으면 감시점이 무장되지 않은 것입니다.
- 각 적중의 `EIP` 앞 바이트 창에 `c7 80 10 0a 00 00 00 00 00 00`이 있어야 합니다.
- 적중이 0건이면 그 자체가 결과이며, 생성자가 실행되지 않았다는 뜻으로 기록합니다.

---

# Task 185: Watching Writes To The `+0xa10` Field

## Goal

Add a hardware write watchpoint on an arbitrary guest address and use it to observe every write to EZ2DJ 4th's faulting field, separating the three hypotheses Task 184 left open.

## Scope

Add `--field-write-watch <hex-address>` requiring four-byte alignment; arm a four-byte write watchpoint in `Dr1` on the first thread and every thread created afterwards; reject the option combinations that also use `Dr1`; record the hit context including the value written, the caller return address, and a byte window before `EIP`; then run the investigation and record what it shows.

## Out Of Scope

Generalizing the existing watch, read or execute breakpoints, watching several addresses at once, and changing guest behavior.

## Verification

Build clean, run the unit tests and product loader probe, confirm the two writes Task 184 located appear as hits with the expected bytes before `EIP`, and compare the hit order against the fault. Zero hits is a result.
