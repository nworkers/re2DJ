# 타깃 프로파일 실행 기본값과 shortcut 작업 로그

## 결과

`ez2dj3rd`를 단순 감지 결과가 아니라 실행 가능한 built-in profile로 확장했다. 프로파일에 기본 HDD 경로와 원본 빌드에서 확인된 Windows 실행 정책을 저장하고, CLI에서 지정한 값이 적용 가능한 범위에서 이를 덮어쓰도록 했다. `re2dj ez2dj3rd`는 저장소 root 기준 `roms/ez2dj3rd`를 자동 선택하고 `ez2dj/EZ2DJ.EXE`를 실행 경로로 사용한다.

*Extended `ez2dj3rd` from a detection-only result into an executable built-in profile. The profile stores its default HDD path and the Windows execution policy supported by the observed original build; applicable CLI values override those defaults. `re2dj ez2dj3rd` selects `roms/ez2dj3rd` relative to the repository root and uses `ez2dj/EZ2DJ.EXE` as the execution path.*

## 변경 사항

- `TargetRunDefaults`를 추가해 shortcut HDD 경로, audio/demo 값, fullscreen과 HLE/detached 정책을 `TargetProfile`에 귀속했다.
- 1st SE의 기존 canonical product-loader 인자를 프로파일 기본값으로 이동했고, 3rd는 확인된 DirectSound ordinal `#1`, 파일 I/O VFS와 detached 실행만 기본 활성화했다.
- 3rd IAT에 없는 `DirectDrawCreate`, display mode, command-line, Windows-directory, DemoVolume, legacy I/O hook은 기본 정책에 넣지 않았다. VFS의 `GetFileType`와 `LoadImageA`는 선택적 import로 처리했다.
- CLI에 positional profile ID를 추가했다. positional ID는 자동으로 실행을 선택하고, `--hdd`는 shortcut 경로를 대체하며, `--target`은 최종 profile ID로 우선한다.
- `--fullscreen`, `--windowed`, audio와 I/O 관련 명령행 값의 적용 가능 여부를 프로파일 기준으로 검증한다. 확인되지 않은 3rd I/O·DemoVolume·display hook은 실행 전에 거절한다.
- 설계 문서, 작업 지시서, 현재 아키텍처, Windows 실행 가이드, HDD 설정 가이드와 3rd PE 분석을 갱신했다.

*Added `TargetRunDefaults` to keep shortcut paths, audio/demo values, fullscreen, and HLE/detached policy with each target profile. Moved the existing 1st SE canonical product-loader arguments into profile defaults and enabled only the confirmed 3rd DirectSound ordinal `#1`, file-I/O VFS, and detached execution by default. Absent 3rd imports are handled conservatively: `GetFileType` and `LoadImageA` are optional VFS hooks, while `DirectDrawCreate`, display mode, command-line, Windows-directory, DemoVolume, and legacy-I/O hooks are not enabled. Positional profile IDs now imply execution; `--hdd` overrides the shortcut path and `--target` determines the final profile ID. Profile capability checks reject unconfirmed 3rd I/O, DemoVolume, and display hooks before launch. Updated the design, work order, architecture, Windows runtime guide, HDD setup guide, and 3rd PE analysis.*

## 검증

| 항목 | 결과 |
| --- | --- |
| Windows x86 Debug build | 통과 |
| `ctest --preset windows-x86-debug` | 3/3 통과 |
| `ctest --preset windows-x86-native-probe` | 1/1 통과 |
| target profile synthetic test | 3rd 기본 경로·정책·미확정 guest path 고정 |
| Windows product-loader probe | 1st 인자 순서와 3rd profile-derived 인자 통과 |
| `re2dj ez2dj3rd` | `roms/ez2dj3rd` 탐색, `ez2dj/EZ2DJ.EXE` 선택, VFS mount 및 `runtime_detached` 확인 |

*The Windows x86 Debug build passed. `ctest --preset windows-x86-debug` passed 3/3, and `ctest --preset windows-x86-native-probe` passed 1/1. Synthetic target-profile tests pin the 3rd shortcut path, policy, and unresolved guest path; the product-loader probe preserves 1st argument order and checks 3rd-derived arguments. The real `re2dj ez2dj3rd` run selected `roms/ez2dj3rd`, resolved `ez2dj/EZ2DJ.EXE`, mounted VFS, and recorded `runtime_detached`.*

실제 실행 로그 `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-152959-334.jsonl`에서 protected 3rd process가 계속 살아 있는 상태까지 확인한 뒤 검증을 위해 수동 종료했다. 이 결과는 shortcut·runtime 준비와 detached 진입을 확인하지만, 3rd의 전체 그래픽 경로, DirectInput/AVI/WS2_32 사용, 보호 해제 성공과 물리 LPTDI 응답을 확인한 것은 아니다. `System.ini`가 없으므로 guest drive letter와 Win32 guest directory도 계속 미확정이다.

*The live protected 3rd process remained running after detachment in `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-152959-334.jsonl` and was stopped manually for verification. This confirms shortcut resolution, runtime preparation, and detached entry, but not complete 3rd graphics execution, DirectInput/AVI/WS2_32 usage, protection success, or the physical LPTDI response. The guest drive letter and Win32 guest directory remain unresolved because `System.ini` is absent.*
