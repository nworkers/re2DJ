# LPTDI 프로파일 분리 작업 로그

관련 설계: [LPTDI 프로파일 분리 설계](../design/20260830-100-lptdi-profile-separation.md)<br>
관련 작업 지시: [LPTDI 프로파일 분리 작업 지시](../work-orders/20260830-100-lptdi-profile-separation.md)

## 결과

**완료.** `ez2dj3rd` 실행을 기준으로 LPTDI 정책을 프로파일별로 분리했다. 1st SE는 기존 확인값을 유지하고, 3rd는 legacy raw I/O·synthetic LPTDI mock·target state를 모두 비활성화한다. 3rd의 `UseIOCard=1`은 장치 계약을 확정하는 근거로 사용하지 않았다.

* **Complete.** LPTDI policy is now separated per profile based on an `ez2dj3rd` execution. 1st SE retains its confirmed values, while 3rd disables legacy raw I/O, the synthetic LPTDI mock, and target state. 3rd `UseIOCard=1` was not treated as proof of a device contract.*

## 실행 및 조사

- 3rd의 제한 명령 추적 `--instruction-trace 2048`은 시스템 DLL 초기화 구간에서 step limit에 도달했으며 LPTDI 호출을 직접 증명하지 않았다. 로그: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-172314-688.jsonl`.
- `--hle-vfs --run-detached` 실행은 entry 주입, `ez2dj` working-directory VFS mount, `runtime_detached`까지 도달했다. 해당 실행에서 LPTDI 응답 이벤트와 3rd VFS asset trace는 관찰되지 않았다. 로그: `20260830-172403-483.jsonl`.
- 1st SE의 target state `0900000000000000`을 3rd에 강제로 전달한 비교는 구현 후 `LPTDI device mock is not configured for this target`로 import 패치 전에 거부됐다. 로그: `20260830-172624-412.jsonl`.
- 제품 shortcut `re2dj ez2dj3rd`를 다시 실행해 `roms/ez2dj3rd`, `ez2dj/EZ2DJ.EXE`, DirectSound ordinal 1, VFS mount, `runtime_detached`를 확인했다. 로그: `20260830-173056-409.jsonl`. 검증 후 생성된 PID 17656 프로세스를 종료했다.
- 3rd EXE 정적 검색에서 `LPTDI`, `TDSD.VXD`, `DeviceIoControl`은 발견되지 않았고, 정적 IAT에는 `DeviceIoControl`이 없다. `EZ2DJ.INI`의 `UseIOCard=1`과 `DirectInputCreateA` import는 별도 미확정 경계로 남겼다.

*The bounded 3rd `--instruction-trace 2048` reached the system-DLL initialization region and hit its step limit; it did not directly prove an LPTDI call. Log: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-172314-688.jsonl`.*
*The `--hle-vfs --run-detached` run reached entry injection, the `ez2dj` working-directory VFS mount, and `runtime_detached`, but no LPTDI response event or 3rd VFS asset trace was observed. Log: `20260830-172403-483.jsonl`.*
*Forcing the 1st SE target state `0900000000000000` onto 3rd was rejected before import patching with `LPTDI device mock is not configured for this target` after the implementation. Log: `20260830-172624-412.jsonl`.*
*The product shortcut `re2dj ez2dj3rd` was rerun and confirmed `roms/ez2dj3rd`, `ez2dj/EZ2DJ.EXE`, DirectSound ordinal 1, VFS mount, and `runtime_detached`. Log: `20260830-173056-409.jsonl`. The created PID 17656 was stopped after verification.*
*Static scanning found no `LPTDI`, `TDSD.VXD`, or `DeviceIoControl` strings in the 3rd EXE, and its static IAT has no `DeviceIoControl`. `UseIOCard=1` in `EZ2DJ.INI` and the `DirectInputCreateA` import remain separate unresolved boundaries.*

## 코드 변경

- `TargetLptdiPolicy`를 `TargetRunDefaults` 아래에 추가해 `legacy_io_ports`, `device_mock_enabled`, `device_mock_target_state_hex`를 묶었다.
- 1st SE profile에만 `legacy_io_ports=true`, `device_mock_enabled=true`, `0900000000000000`을 설정했다.
- Windows original-process backend가 중첩 정책을 검증하고 launcher 인자로 변환하도록 갱신했다. 정책이 켜졌는데 target state가 없거나, state만 남은 불일치도 거부한다.
- launcher가 3rd에 `--device-mock-lptdi*`를 받으면 `DeviceIoControl` 슬롯을 찾기 전에 profile policy 오류를 반환한다.
- target profile 및 product-loader probe에서 1st/3rd 차이와 3rd 인자 누락을 고정했다.
- 설계·작업 지시·analysis·`ARCHITECTURE.md`를 갱신했다. 원본 HDD와 실행 파일은 수정하거나 저장소에 추가하지 않았다.

*Added `TargetLptdiPolicy` under `TargetRunDefaults` to group `legacy_io_ports`, `device_mock_enabled`, and `device_mock_target_state_hex`.*
*Only the 1st SE profile sets `legacy_io_ports=true`, `device_mock_enabled=true`, and `0900000000000000`.*
*Updated the Windows original-process backend to validate and translate the nested policy into launcher arguments, rejecting both an enabled policy without state and a state without device-mock enablement.*
*The launcher now returns a profile-policy error for 3rd `--device-mock-lptdi*` before looking for the `DeviceIoControl` slot.*
*Pinned the 1st/3rd difference and omitted 3rd arguments in the target-profile and product-loader probes.*
*Updated the design, work order, analysis, and `ARCHITECTURE.md`. No original HDD or executable was modified or added to the repository.*

## 검증

- `cmake --build --preset windows-x86-debug --config Debug` 통과
- `ctest --preset windows-x86-debug --output-on-failure` 통과: 3/3
- `re2dj.exe ez2dj3rd --list-targets` 통과: `roms/ez2dj3rd`, `ez2dj/EZ2DJ.EXE`, built-in profile
- 3rd 강제 LPTDI mock 거부 확인
- `git diff --check` 통과
- 실행 후 새 `EZ2DJ`/launcher 프로세스 없음 확인

*Verification passed `cmake --build --preset windows-x86-debug --config Debug`; `ctest --preset windows-x86-debug --output-on-failure` with 3/3 tests; `re2dj.exe ez2dj3rd --list-targets` with `roms/ez2dj3rd`, `ez2dj/EZ2DJ.EXE`, and the built-in profile; the forced 3rd LPTDI-mock rejection; `git diff --check`; and the post-run check that no new EZ2DJ/launcher process remained.*

## 미확정 사항

3rd의 `UseIOCard`가 실제로 어떤 입력 경계와 연결되는지, 보호 코드가 동적 해석이나 다른 장치 경로를 사용하는지, LPTDI 응답 계약과 물리 동글 알고리즘은 확인하지 않았다. 향후 증거가 생기면 3rd 정책을 별도 값으로 추가하며 1st target state를 재사용하지 않는다.

*The actual input boundary behind 3rd `UseIOCard`, any dynamic or alternate device path used by its protection code, the LPTDI response contract, and the physical-dongle algorithm remain unconfirmed. If future evidence appears, it will add separate 3rd policy values rather than reusing the 1st target state.*
