# KSND search-path 관찰 작업 로그

관련 설계: [KSND search-path와 파일 후보 관찰](../design/20260825-064-ksnd-search-path-observation.md)

관련 작업 지시: [KSND search-path 관찰 작업 지시](../work-orders/20260825-064-ksnd-search-path-observation.md)

## 결과

확인된 KSND controlled-exit caller에서 원본 search-path global을 bounded read하도록 launcher를 확장했다. Windows x86 Debug build와 CTest 2/2가 통과했다.

최종 로그는 다음 두 개다.

- `20260825-015354-326.jsonl`
- `20260825-015423-475.jsonl`

두 실행 모두 count 1, entry `System/Common`, caller `0x00424813`, detail `coin0.wav`를 기록했다. `--api-trace`는 runtime wrapper가 host `CreateFileA`에 전달한 경로를 `roms/ez2dj1stse/System/Common/coin0.wav`로 확인했다. 실제 read-only HDD 파일은 `roms/ez2dj1stse/ez2dj/System/Common/coin0.wav`에 있으므로 target working directory가 VFS mount root에 반영되지 않은 것이 원인이다. 두 로그 모두 `av_access`가 없고 원본 자산은 변경하지 않았다.

다음 작업은 target profile의 `working_directory_relative_path`를 사용해 VFS mount root를 계산하고, overlay 상대 경로 정책도 같은 guest root 기준을 유지하는 설계다.

---

# KSND Search-Path Observation Work Log

Related design: [KSND Search-Path and File-Candidate Observation](../design/20260825-064-ksnd-search-path-observation.md)

Related work order: [KSND Search-Path Observation Work Order](../work-orders/20260825-064-ksnd-search-path-observation.md)

The launcher now performs a bounded read of the original search-path globals only at the confirmed KSND controlled-exit caller. Windows x86 Debug builds and CTest passes 2/2.

Final logs `20260825-015354-326.jsonl` and `20260825-015423-475.jsonl` both record count one, entry `System/Common`, caller 0x00424813, detail `coin0.wav`, and no access violation. API tracing confirms the runtime passes `roms/ez2dj1stse/System/Common/coin0.wav` to host CreateFileA, while the read-only asset actually resides under `roms/ez2dj1stse/ez2dj/System/Common/coin0.wav`. The cause is the missing target working-directory component in the VFS mount root. No original asset was changed.
