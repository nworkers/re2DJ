# Windows Original Import Observer

## 한국어

### 목적

사용자가 제공한 HDD의 bring-up target `ez2dj1.exe`를 Windows Win32 helper에서 preferred base `0x00400000`으로 실행하고, HLE 구현 전 실제 첫 import gate 호출을 관찰합니다.

### 안전 경계

관찰기는 import gate에서 module/name/ordinal, instruction pointer, stack pointer, event ID를 기록한 뒤 `ImportCompletionAction::kStop`으로 helper를 종료합니다. guest import를 성공한 것처럼 계속 실행하거나 원본 HDD에 쓰지 않습니다. 따라서 관찰 범위는 helper가 첫 import에 도달하는지와 호출 순서 확인으로 제한됩니다.

### 입력과 출력

입력은 HDD root, target profile ID, Windows helper executable path입니다. `HddRoot`와 target profile을 사용해 executable을 해석하며, file bytes는 helper protocol로만 전달합니다. 출력은 한 event당 JSON Lines 한 줄이고, 관찰 실패·PE parsing 실패도 구조화된 오류로 보고합니다.

### 확인 상태

`ez2dj1.exe`가 preferred base에 고정되어 있다는 사실은 HDD 분석에서 확인됐다. 첫 import 호출 순서와 stack argument 의미는 이 작업에서 관찰하여 확인한다.

## English

### Purpose

Run the user-supplied HDD bring-up target `ez2dj1.exe` in the Windows Win32 helper at preferred base `0x00400000`, then observe its first real import-gate call before implementing HLE APIs.

### Safety boundary

At an import gate, the observer records module/name/ordinal, instruction pointer, stack pointer, and event ID, then terminates the helper with `ImportCompletionAction::kStop`. It neither continues a guest import as successful nor writes to the original HDD.

### Input and output

Inputs are HDD root, target-profile ID, and Windows helper executable path. The observer uses `HddRoot` and target profiles to resolve the executable and sends only file bytes through the helper protocol. Output is JSON Lines, one line per event, with structured errors for observation and PE parsing failures.

### Confirmation status

HDD analysis confirms `ez2dj1.exe` is fixed at its preferred base. The first import-call order and stack-argument meaning are confirmed by this work.
