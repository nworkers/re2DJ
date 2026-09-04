# Task 175: EZ2DJ 4th 초기화되지 않은 out 인자 추적

## 작업 목표

`RVA 0x00009701`의 접근 위반 원인인 `0xcccccccc` 인자가 어느 호출에서 채워졌어야 하는지 특정합니다.

## 선행 문서

- [Task 175 설계](../design/20260904-175-ez2dj4th-uninitialized-out-parameter.md)
- [Task 174 작업 로그](../work-logs/20260904-174-ez2dj4th-io-out-helper.md)
- [Task 173 작업 로그](../work-logs/20260904-173-ez2dj4th-ddcaps2-windowed.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **코드 범위 교체.** 참조 스캔의 `code_ranges`에서 목적을 다한 `driver_stage`를 빼고 `av_callee`(`0x00009690`, `0x100`), `av_caller`(`0x00065700`, `0x220`), `link_thunks`(`0x00001840`, `0x40`)를 넣습니다.
2. **진단 실행과 판정.** 참조 스캔을 실행해 세 구간을 덤프하고, 싱글턴 메서드의 실제 주소와 out 인자를 쓰는 조건을 복원합니다.
3. **자원 이름 창.** 실패한 적재 요청의 이름을 만드는 `.rdata` 문자열 구간을 데이터 창으로 읽습니다.
4. **CHD 디렉터리 나열.** `re2dj_chd_probe`에 `--list <relative-directory>` 옵션을 추가해, 실패한 자원이 이미지 안 어디에 있는지 확인합니다. 상대 경로 열기가 실패했을 때 파일이 실제로 어느 디렉터리에 있는지는 이미지만 답할 수 있으므로, 반복 사용 가능한 진단으로 둡니다.
5. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 갱신하고 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- 관측 전 HLE facade 수정.
- 게스트 코드 patch 또는 지역 값 주입.
- 새 CLI 옵션 추가.
- `direct3d3_com_facade`(DirectX 6 경로) 변경.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 참조 스캔을 확장 idle 경계로 실행합니다.

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-object-reference-scan
```

## 자기 검증 기준

- `av_callee` 덤프의 시작 바이트가 `558bec51c745fc`이면 범위와 정렬이 맞는 것입니다.
- `av_caller` 덤프 안에서 `0x004658d7`의 `e8 fccaf9ff`가 보이면 호출 지점을 포함한 것입니다.
- 로그에는 원본 자산 내용과 Hardlock secret 값을 기록하지 않습니다.

---

# Task 175: Tracing the EZ2DJ 4th Uninitialized Out-Parameter

## Goal

Identify which call should have filled the `0xcccccccc` argument behind the access violation at `RVA 0x00009701`.

## Scope

1. Replace the spent `driver_stage` code range with `av_callee` (`0x00009690`, `0x100`), `av_caller` (`0x00065700`, `0x220`), and `link_thunks` (`0x00001840`, `0x40`).
2. Run the reference scan, recover the singleton method's real address, and recover the condition under which it writes its out parameter.
3. Update the work log and the analysis topic, marking each statement confirmed, inferred, or unresolved.

## Out of Scope

Facade changes before observation, guest patching or local injection, new CLI options, and DirectX 6 path changes.

## Minimum Verification

Build, unit tests, and the product loader probe, then the reference scan against the real CHD.

## Self-Check

An `av_callee` dump starting with `558bec51c745fc` confirms range and alignment, and seeing `e8 fccaf9ff` at `0x004658d7` inside the `av_caller` dump confirms the call site is covered. Logs never record original asset contents or Hardlock secrets.

---

## 범위 확장 기록 (Scope Extension Record)

관측 도중 실패한 호출이 자원 적재였다는 것이 드러나, 두 항목을 범위에 추가했습니다.

1. 자원 이름을 담은 `.rdata` 구간 데이터 창.
2. `re2dj_chd_probe --list <relative-directory>`. 상대 경로 열기 실패의 원인을 가리려면 이미지 안의 실제 디렉터리 배치를 봐야 하고, 이 질문은 앞으로도 반복되므로 일회성 코드가 아니라 도구 옵션으로 남깁니다.

While observing, the failing call turned out to be a resource load, so two items were added to scope: a data window over the `.rdata` names, and a `--list` option on `re2dj_chd_probe`. Telling apart the causes of a failed relative open needs the image's real directory layout, and that question recurs, so it becomes a tool option rather than throwaway code.
