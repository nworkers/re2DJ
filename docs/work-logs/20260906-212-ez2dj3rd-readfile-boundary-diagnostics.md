# 작업 로그: EZ2DJ 3rd ReadFile 경계 진단

## 한국어

### 수행 내용

- `main`에서 `task-212-ez2dj3rd-readfile-boundary-diagnostics` 브랜치를 생성했습니다.
- Windows injected VFS에 bounded `ReadFile` 진입/결과 로그를 추가했습니다.
- native, CHD, device-mock 경로를 구분하고, 파일 내용과 Hardlock 자료는 기록하지 않도록 했습니다.
- `ARCHITECTURE.md`와 3rd Hardlock 분석 문서에 계측 경계와 관찰 결과를 반영했습니다.

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- `ctest --test-dir build/windows-x86 -C Debug -R "re2dj_unit_tests|re2dj_windows_product_loader_probe" --output-on-failure` 성공, 2/2
- 사용자와 동일한 명령으로 `ez2dj3rd` 실행:
  `re2dj.exe ez2dj3rd --io-config .\config\ez2dj-io.example.ini`
- 로그: `logs/windows_x86_launcher_probe/ez2dj3rd/20260906-202402-449.vfs.log`

### 결과

모든 초기 파일 읽기는 성공적으로 반환되었습니다. 따라서 이번 실행에서는 `ReadFile` 내부 대기를 원인으로 볼 근거가 없습니다. 실행은 Hardlock transform 32회까지 진행했고, 28개 map 항목을 모두 매핑한 뒤 descriptor function `0x0001` 요청 이후 `0xc0000096`으로 종료했습니다.

현재 가장 중요한 미확정 사항은 3rd 프로파일에서 legacy I/O를 비활성화한 상태의 종료 코드가 실제 raw-I/O 실행 때문인지, function `0x0001`의 미확인 driver 응답 때문인지입니다. 이 작업에서는 프로파일 기본값과 Hardlock 응답을 변경하지 않았습니다.

### 보존 사항

작업 시작 시부터 존재하던 `config/ez2dj-io.example.ini`의 사용자 변경은 커밋 대상에서 제외하고 그대로 보존했습니다.

## English

### Work performed

- Created branch `task-212-ez2dj3rd-readfile-boundary-diagnostics` from `main`.
- Added bounded `ReadFile` entry/result records to the Windows injected VFS.
- Distinguished native, CHD, and device-mock paths without recording file contents or Hardlock material.
- Updated `ARCHITECTURE.md` and the 3rd Hardlock analysis with the instrumentation boundary and observation.

### Verification

- `cmd /c scripts\build_win32.bat` succeeded.
- Focused CTest run passed 2/2 tests.
- Ran the user's `ez2dj3rd --io-config` command.
- Trace: `logs/windows_x86_launcher_probe/ez2dj3rd/20260906-202402-449.vfs.log`.

### Result

All initial file reads returned successfully, so this run provides no evidence of a wait inside `ReadFile`. The process reached 32 Hardlock transforms, mapped all 28 configured entries, then exited with `0xc0000096` after a descriptor function `0x0001` request.

The key unresolved distinction is whether this exit while 3rd legacy I/O is disabled reflects an actual raw-I/O instruction or a missing real driver response for function `0x0001`. This task did not change the profile default or guess a Hardlock response.

### Preserved change

The pre-existing user modification to `config/ez2dj-io.example.ini` was excluded from the commit and left untouched.

### 후속 privileged-instruction 확인

추가로 unhandled `EXCEPTION_PRIV_INSTRUCTION`을 crash-context 형식으로 기록하는 계측을 적용하고 다시 빌드·테스트했습니다. 후속 실행 `20260906-203411-416`에는 privileged-instruction 예외 기록이 없었고, `exit-process` 기록에는 guest image 내부의 `ExitProcess` import 호출 직전 바이트와 `0xc0000096` 코드가 남았습니다. 따라서 이 실행에서 injected exception handler로 전달된 raw-I/O fault는 직접 관찰되지 않았습니다.

### Follow-up privileged-instruction check

Added observational logging for unhandled `EXCEPTION_PRIV_INSTRUCTION`, then rebuilt and reran the focused tests. Follow-up run `20260906-203411-416` contained no privileged-instruction exception record; its `exit-process` record retained the bytes immediately before the guest image's `ExitProcess` import call and code `0xc0000096`. An unhandled raw-I/O fault reaching the injected exception handler was therefore not directly observed in this run.
