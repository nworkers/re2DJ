# 프로파일별 Hardlock/LPTDI 응답 작업 로그

## 요약

1st SE의 LPTDI challenge-response 변환을 공용 runtime에 유지하면서, 프로파일마다 synthetic device path와 post-XOR target state를 분리했습니다. 1st SE는 `\\.\\LPTDI`와 `0900000000000000`, 3rd는 `\\.\\Hardlock`과 `0000000000000000` zero-state probe를 사용합니다. 3rd 값은 실행을 위한 진단 후보일 뿐 실제 Hardlock seed나 유효 동글 응답으로 확정하지 않았습니다.

*The shared runtime keeps the 1st SE LPTDI challenge-response transform while separating the synthetic device path and post-XOR target state per profile. 1st SE uses `\\.\\LPTDI` and `0900000000000000`; 3rd uses `\\.\\Hardlock` and a `0000000000000000` zero-state probe. The 3rd value is a diagnostic candidate for execution, not a confirmed Hardlock seed or valid dongle response.*

## 구현

- `TargetLptdiPolicy`에 `device_mock_path_prefix`를 추가하고 1st SE와 3rd의 path/state를 독립적으로 설정했습니다.
- Windows original-process backend가 선택된 profile 값을 `--device-mock-lptdi-path-prefix`와 `--device-mock-lptdi-target-state`로 launcher에 전달합니다.
- injected runtime이 profile prefix를 case-insensitive로 매칭하고, 정적 IAT에 없는 3rd의 동적 `GetProcAddress` 요청도 `CreateFileA`, 파일 wrapper, `DeviceIoControl` wrapper로 연결합니다.
- product loader probe가 1st/3rd 인자의 분리 및 command-line override 순서를 검사하고, VFS runtime probe가 `Hardlock` prefix와 동적 wrapper를 검사합니다.

*Added `device_mock_path_prefix` to `TargetLptdiPolicy` and configured independent 1st SE and 3rd path/state values. The Windows original-process backend passes the selected profile values to the launcher as `--device-mock-lptdi-path-prefix` and `--device-mock-lptdi-target-state`. The injected runtime matches the selected prefix case-insensitively and routes 3rd's dynamically resolved `GetProcAddress` requests for `CreateFileA`, file wrappers, and `DeviceIoControl` through the HLE wrappers. The product-loader probe checks separated 1st/3rd arguments and override ordering, while the VFS runtime probe checks the Hardlock prefix and dynamic wrappers.*

## 실행 및 검증

- Windows x86 Debug 빌드:

  `cmake --build --preset windows-x86-debug --config Debug --target re2dj_windows_vfs_runtime_probe re2dj_windows_x86_launcher_probe re2dj_windows_product_loader_probe re2dj_unit_tests`

- CTest: 3/3 통과 (`re2dj_windows_vfs_runtime_probe`, `re2dj_windows_product_loader_probe`, `re2dj_unit_tests`).
- 제품 실행: `build/windows-x86/bin/Debug/re2dj.exe ez2dj3rd`.
- 실행 로그: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-215014-756.jsonl`.
- 로그는 `ez2dj/EZ2DJ.EXE` 선택, runtime 주입, DirectSound hook, VFS mount, profile target-state 전달과 `runtime_detached`를 기록했습니다. 원본 프로세스는 6초 bounded 관찰 동안 응답 상태였고, 이전 `Hardlock` 오류 대화상자는 관찰되지 않았습니다.
- 해당 실행에서 `.vfs.log`는 생성되지 않았습니다. 따라서 실제 3rd 동적 Hardlock 요청이 wrapper를 호출했는지, zero-state가 보호 검사를 통과하는지, 게임 화면까지 도달하는지는 미확정으로 남깁니다.
- 검증 후 이번 실행으로 시작한 원본 및 launcher 프로세스를 종료했습니다.

*Windows x86 Debug build: `cmake --build --preset windows-x86-debug --config Debug --target re2dj_windows_vfs_runtime_probe re2dj_windows_x86_launcher_probe re2dj_windows_product_loader_probe re2dj_unit_tests`.*

*CTest passed all 3 tests: `re2dj_windows_vfs_runtime_probe`, `re2dj_windows_product_loader_probe`, and `re2dj_unit_tests`. Product command: `build/windows-x86/bin/Debug/re2dj.exe ez2dj3rd`. Verification log: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-215014-756.jsonl`. The log records selection of `ez2dj/EZ2DJ.EXE`, runtime injection, DirectSound hook, VFS mount, profile target-state transfer, and `runtime_detached`. The original process remained responsive during a six-second bounded observation, and the previous `Hardlock` error dialog was not observed. No `.vfs.log` was created, so whether the real dynamic 3rd Hardlock request called the wrapper, whether zero state passes protection, and whether the game reaches its screen remain unresolved. The original and launcher processes started for this run were terminated after verification.*

## 판단

Hardlock과 LPTDI를 별도 알고리즘으로 복제하지 않고, 확인된 IOCTL shape와 challenge-mask 변환은 공용으로 두며 경로와 콘텐츠별 target state만 profile data로 분리하는 구조가 현재 증거에 맞습니다. 실제 3rd seed/response가 확인되면 `TargetLptdiPolicy`의 3rd state만 교체할 수 있습니다.

*The evidence supports sharing the confirmed IOCTL shape and challenge-mask transform while keeping only the device path and content-specific target state in profile data, rather than duplicating separate Hardlock and LPTDI algorithms. Once the real 3rd seed/response is confirmed, only the 3rd state in `TargetLptdiPolicy` needs to be replaced.*

원본 HDD, 실행 파일과 게임 데이터는 저장소에 추가하거나 수정하지 않았습니다.

*No original HDD contents, executable, or game data was added to or modified in the repository.*
