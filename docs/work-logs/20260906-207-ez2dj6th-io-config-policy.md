# ez2dj6th I/O 설정 전달 오류 수정 작업 로그

## 원인

`ez2dj6th` profile은 bootstrap 자식 추적을 위해 `follow_child_process=true`와
`run_detached=false`를 사용한다. 동시에 4th의 raw-I/O helper가 확인되지 않아
`legacy_io_ports=false`이다. 그러나 CHD 제품 CLI가 사용자가 지정한 `--io-config`를
그대로 Windows launcher에 전달했다. launcher의 `--io-config` 검증은 detached raw-I/O
경로만 허용하므로 Usage를 출력하고 bootstrap을 시작하지 않았다.

## 변경

* profile의 `legacy_io_ports` capability를 기준으로 제품 CLI가 `io_config`를 정리한다.
* 6th처럼 capability가 없는 profile은 옵션을 stderr로 안내한 뒤 launcher 인자에서
  제거한다.
* Windows original-process backend는 capability와 맞지 않는 직접 `io_config` 호출을
  오류로 거부한다.
* 설계와 재현/검증 절차를 bilingual 문서로 남겼다.

## 검증

* `cmd /c scripts\build_win32.bat` 성공
* `build\windows-x86\bin\Debug\re2dj_unit_tests.exe` 성공: `1374 checks, 0 failures`
* `re2dj_windows_product_loader_probe.exe` 성공. 6th 직접 backend 호출은
  capability 불일치 `io_config`를 거부하고, 정리된 인자에는 `--io-config` 없이
  `--follow-child`가 유지되는 것을 확인
* 실제 명령
  `re2dj.exe ez2dj6th --io-config .\config\ez2dj-io.example.ini`을 10초 bounded
  실행. stderr에 무시 안내가 출력되고 launcher JSONL에는
  `follow_child=true`, `hle_io_ports=false`, `vfs_mount`, dynamic resolver,
  `CreateFileA`/`DeviceIoControl` 이벤트가 기록되었다. 기존 launcher Usage는
  재현되지 않았다. Hardlock/부트 경계 이후의 대기는 테스트 프로세스만 종료했다.

원본 CHD 자산과 Hardlock 응답은 저장소에 기록하지 않으며, 이 변경은 6th raw-I/O
helper나 응답을 확정하지 않는다.

---

# Work Log: Fix ez2dj6th I/O Configuration Forwarding

## Cause

The `ez2dj6th` profile follows the bootstrap child with
`follow_child_process=true` and `run_detached=false`. It also leaves
`legacy_io_ports=false` because the 4th raw-I/O helper is not confirmed for 6th.
The CHD product CLI nevertheless forwarded the user's `--io-config` unchanged.
The launcher only accepts that option for detached raw-I/O execution, so it printed
Usage and never started the bootstrap.

## Changes

* The product CLI normalizes `io_config` using the profile's `legacy_io_ports`
  capability.
* Profiles without that capability, including 6th, report that the option is ignored
  and omit it from launcher arguments.
* The Windows original-process backend rejects direct incompatible `io_config`
  requests.
* The design and reproducible verification procedure are recorded bilingually.

## Verification

* `cmd /c scripts\build_win32.bat` succeeded.
* `build\windows-x86\bin\Debug\re2dj_unit_tests.exe` succeeded:
  `1374 checks, 0 failures`.
* `re2dj_windows_product_loader_probe.exe` succeeded. A direct 6th backend call
  rejects capability-incompatible `io_config`, while normalized arguments retain
  `--follow-child` without `--io-config`.
* A 10-second bounded run of
  `re2dj.exe ez2dj6th --io-config .\config\ez2dj-io.example.ini` printed the
  ignore notice and recorded `follow_child=true`, `hle_io_ports=false`, `vfs_mount`,
  the dynamic resolver, and `CreateFileA`/`DeviceIoControl` events in launcher
  JSONL. The previous launcher Usage was not reproduced. The test process was
  stopped only after it waited beyond the unresolved Hardlock/boot boundary.

Original CHD assets and Hardlock responses are not recorded in the repository. This
change does not confirm 6th raw-I/O helper RVAs or response material.
