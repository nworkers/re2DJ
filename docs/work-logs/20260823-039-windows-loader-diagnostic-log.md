# Windows 로더 진단 로그 작업 로그

관련 작업 지시: [Windows 로더 진단 로그 작업 지시](../work-orders/20260823-039-windows-loader-diagnostic-log.md)  
관련 설계: [Windows 로더 진단 로그](../design/20260823-039-windows-loader-diagnostic-log.md)

## 구현 결과

`re2dj_windows_x86_launcher_probe`에 실행별 JSONL 진단 로그를 추가했다. 대상 프로필과 원본 실행 파일이 해석되면 launcher는 다음 경로에 파일을 만들고 각 기록 뒤 즉시 flush한다.

`logs/windows_x86_launcher_probe/<target-id>/<timestamp>.jsonl`

로그에는 `launch`, 모든 debug event, 예외 메모리 영역·스택·바이트 관찰값, `ExitProcess` 관찰값, 그리고 `outcome`이 포함된다. `--trace`를 사용하지 않아도 파일에는 기록하며, stderr에는 실패 `error`와 `diagnostic_log` 경로를 출력한다. 성공 stdout에도 같은 경로를 포함한다. 생성 디렉터리 `logs/`는 Git ignore에 추가했다.

## 검증 결과

1. `cmake --build --preset windows-x86-debug --config Debug` 성공.
2. `ctest --test-dir build\\windows-x86 -C Debug --output-on-failure` 성공: 2/2 테스트 통과.
3. `ez2dj1stse`의 canonical `ez2dj.exe`를 `--software-breakpoint --break-exit-process`로 실행했다.
   - `--trace` 없이 stderr가 다음처럼 로그 경로를 반환했다.
   - `{"error":"original process exited with code 0xc000001d before ExitProcess breakpoint","diagnostic_log":".../logs/windows_x86_launcher_probe/ez2dj1stse/20260823-010035-971.jsonl"}`
   - 해당 파일에는 `launch`부터 `EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D)` 및 최종 `outcome`까지 기록됐다.

이번 검증은 기존 결론을 변경하지 않는다. 보호된 실행 파일은 static entry 이후 private read/write 영역에서 illegal instruction으로 종료했으며, 그 간접 제어 흐름의 원인은 계속 미확정이다. 이제 재현할 때마다 이 근거가 파일로 보존된다.

---

# Windows Loader Diagnostic Log Work Log

Related work order: [Windows Loader Diagnostic Log Work Order](../work-orders/20260823-039-windows-loader-diagnostic-log.md)  
Related design: [Windows Loader Diagnostic Log](../design/20260823-039-windows-loader-diagnostic-log.md)

## Implementation result

Added a per-run JSONL diagnostic log to `re2dj_windows_x86_launcher_probe`. Once the target profile and original executable are resolved, the launcher creates a file at the following location and flushes after every record:

`logs/windows_x86_launcher_probe/<target-id>/<timestamp>.jsonl`

The log contains `launch`, every debug event, exception-region/stack/byte observations, `ExitProcess` observations, and `outcome`. It records to the file without `--trace`; stderr prints the failure `error` and `diagnostic_log` path, and successful stdout includes the same path. The generated `logs/` directory is Git-ignored.

## Verification result

1. `cmake --build --preset windows-x86-debug --config Debug` succeeded.
2. `ctest --test-dir build\\windows-x86 -C Debug --output-on-failure` succeeded: 2/2 tests passed.
3. Ran canonical `ez2dj.exe` in `ez2dj1stse` with `--software-breakpoint --break-exit-process`.
   - Without `--trace`, stderr returned the log path as shown above.
   - The file contains records from `launch` through `EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D)` and final `outcome`.

This verification does not change the existing conclusion. The protected executable still exits from an illegal instruction in a private read/write region after static entry, while the cause of that indirect control flow remains unresolved. The evidence is now preserved for every reproduction run.
