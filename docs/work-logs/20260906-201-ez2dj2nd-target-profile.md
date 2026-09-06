# ez2dj2nd 타깃 프로파일 작업 로그

## 결과

`roms/ez2dj2nd`의 실제 HDD 디렉터리를 `ez2dj2nd` built-in 프로파일로 등록했다. `ez2dj1stse`의 HLE·실행 기본값을 호환성 기준으로 복제했으며, `System.ini`가 없는 2nd 덤프의 게스트 드라이브와 부트 디렉터리는 비워 두었다.

## 확인된 원본 입력

- 대표 실행 파일: `roms/ez2dj2nd/ez2dj/EZ2DJ.exe`
- 스캔 결과: 150 디렉터리, 16,382 파일, PE 실행 파일 1개
- PE32/i386, image base `0x00400000`
- EntryPoint RVA `0x00079550`, `.text` 섹션
- SizeOfImage `0x0047d000`, 5개 섹션
- fingerprint sibling: `EZ2DJ.ini`, `bg`, `sound`, `system`
- `System.ini`: 없음

위 내용은 파일 내용이나 원본 자산을 저장하지 않고 도구 출력으로만 확인했다. EntryPoint가 `.text`라는 사실만으로 Hardlock 또는 다른 보호 계층의 부재를 결론 내리지 않았다.

## 변경 사항

- `src/target/target_profile.cpp`
  - `ez2dj2nd` 디렉터리 프로파일 추가
  - PE EntryPoint/SizeOfImage와 sibling 네 항목을 fingerprint로 등록
  - 1st SE의 command-line, Windows directory, VFS, D3D3, DirectSound, legacy I/O 및 LPTDI mock 기본값을 호환성 기준으로 복제
  - 2nd 전용 Hardlock·legacy I/O·게스트 부트 계약은 미확정으로 note에 표시
- `tests/unit/target_profile_test.cpp`
  - 2nd PE fingerprint와 sibling layout 합성 테스트 추가
  - 실제 실행 파일 상대 경로와 `ez2dj` 작업 디렉터리 확인
  - 게스트 경로를 1st SE에서 잘못 상속하지 않는지 확인
- `docs/design/20260906-201-ez2dj2nd-target-profile.md`
- `docs/work-orders/20260906-201-ez2dj2nd-target-profile.md`
- `docs/analysis/ez2dj-hdd-layout.md`
- `docs/analysis/ez2dj-exe-structures.md`
- `docs/analysis/README.md`
- `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`
- `ARCHITECTURE.md`
- `src/platform/windows/README.md`

원본 `roms/ez2dj2nd` 디렉터리는 변경하지 않았다.

## 검증

### 성공

- `cmd /c scripts\build_win32.bat`
- `build\windows-x86\bin\Debug\re2dj_unit_tests.exe`
  - `checks: 1318, failures: 0`
- `ctest --test-dir build/windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`
  - 1/1 passed
- `re2dj.exe --hdd .\roms\ez2dj2nd --target ez2dj2nd --list-targets`
  - `ez2dj2nd ez2dj/EZ2DJ.exe built-in`

### 범위 외 전체 CTest 실패

전체 3개 CTest를 실행했을 때 새 프로파일을 사용하지 않는 기존 환경 의존 probe 2개가 실패했다.

- `re2dj_windows_vfs_runtime_probe`: `audio backend blocked native process exit (error 0)`
- `re2dj_windows_product_loader_probe`: `failed chd-handoff (last error: invalid Windows original-process options)`

새로 추가한 `re2dj_unit_tests`는 별도로 통과했으며, 위 두 probe는 2nd 프로파일 매칭을 실행하지 않는다. 따라서 이 작업의 프로파일 구현 검증은 성공했지만, 전체 CTest 상태는 별도 환경/기존 probe 문제로 완전 통과하지 못했다.

## 미확정 후속 항목

2nd를 실제 `--run`으로 진입시키려면 1st SE 값을 그대로 신뢰하지 말고, 원본 실행 중 `id_ref`, `id_verify`, Hardlock device path, legacy I/O helper 주소와 응답 계약을 별도로 관찰해야 한다.

---

# ez2dj2nd Target Profile Work Log

## Result

The actual HDD directory under `roms/ez2dj2nd` is now registered as the built-in `ez2dj2nd` profile. The `ez2dj1stse` HLE and execution defaults were copied as a compatibility baseline, while the guest drive and boot directory remain empty because the 2nd dump has no `System.ini`.

## Confirmed original input

- Representative executable: `roms/ez2dj2nd/ez2dj/EZ2DJ.exe`
- Scan: 150 directories, 16,382 files, one PE executable
- PE32/i386, image base `0x00400000`
- Entry RVA `0x00079550` in `.text`
- SizeOfImage `0x0047d000`, five sections
- Fingerprint siblings: `EZ2DJ.ini`, `bg`, `sound`, `system`
- `System.ini`: absent

These facts were confirmed from tool output without storing file contents or original assets. The `.text` entry point alone does not establish that Hardlock or another protection layer is absent.

## Changes

- Added the `ez2dj2nd` directory profile, PE fingerprint, four sibling conditions, and compatibility defaults.
- Added synthetic matching tests for the PE fingerprint, sibling layout, executable-relative path, working directory, and unresolved guest path.
- Updated the design, work-order, cumulative analysis, executable-design, architecture, and Windows platform README documents.
- Left the original `roms/ez2dj2nd` directory untouched.

## Verification

- Windows x86 build succeeded with `cmd /c scripts\build_win32.bat`.
- Unit tests: `checks: 1318, failures: 0`.
- Targeted CTest: `re2dj_unit_tests`, 1/1 passed.
- Real scan: `re2dj.exe --hdd .\roms\ez2dj2nd --target ez2dj2nd --list-targets` selected `ez2dj/EZ2DJ.exe` as built-in.
- The two other full-CTest probes failed outside this profile's scope: the VFS audio-exit probe reported a blocked native exit, and the product-loader probe reported invalid Windows original-process options during CHD handoff. They do not exercise the 2nd profile.

## Unresolved follow-up

Before relying on `--run` for 2nd, independently observe `id_ref`, `id_verify`, the Hardlock device path, legacy-I/O helper addresses, and the response contract instead of treating the copied 1st SE values as confirmed.
