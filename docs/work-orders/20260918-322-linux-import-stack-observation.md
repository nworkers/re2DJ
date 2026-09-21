# 작업 322: Linux 첫 import 스택 관찰 / Task 322: Linux first-import stack observation

## 목표 / Goal

`ez2dj4th` Linux 첫 import gate의 반환 슬롯과 첫 호출자 워드를 실제로 기록한다. 작업 321에서 확인한 `kernel32.dll!GetModuleHandleA` gate를 completion 없이 중지하는 정책은 유지한다.

*Record the return slot and first caller word at the actual Linux first-import gate for `ez2dj4th`. Retain the Task 321 policy of stopping the confirmed `kernel32.dll!GetModuleHandleA` gate without completion.*

## 변경 계획 / Change plan

1. `OriginalRunResult`에 import-stack 관찰 여부, return slot, arg0 필드를 추가한다.
2. Linux runner가 알려진 import gate를 받은 즉시 guest `ESP`에서 8바이트를 읽어 little-endian 값으로 저장한다.
3. CHD 및 directory HDD Linux CLI 출력이 import 이름과 관찰값을 함께 표시하게 한다.
4. x64/x86 product build, 실제 4th CHD 실행, native-helper synthetic probe를 검증한다.
5. 결과를 Linux runtime baseline, architecture, 작업 로그에 확인됨/미확정 상태로 기록한다.

*1. Add import-stack observation presence, return-slot, and arg0 fields to `OriginalRunResult`.
2. Have the Linux runner read eight bytes at guest `ESP` immediately after receiving a known import gate and store the little-endian values.
3. Make both CHD and directory-HDD Linux CLI output show the import name and observations.
4. Validate x64/x86 product builds, actual 4th CHD runs, and the native-helper synthetic probe.
5. Record confirmed and unresolved results in the Linux runtime baseline, architecture, and work log.*

## 완료 기준 / Completion criteria

- 관찰 읽기는 import completion 이전에 한 번만 수행하며 실패 시 helper를 중지하고 오류를 반환한다.
- 실제 x64와 x86 CLI 출력이 첫 import와 두 32비트 관찰값을 보여 준다.
- synthetic probe가 두 호스트 아키텍처에서 통과한다.
- API 반환값이나 이후 실행 성공을 주장하지 않는다.

*The observation read occurs once before import completion and stops the helper with an error on failure. Actual x64 and x86 CLI output shows the first import and two 32-bit observations. The synthetic probe passes on both host architectures. The task makes no claim about API return values or later execution success.*
