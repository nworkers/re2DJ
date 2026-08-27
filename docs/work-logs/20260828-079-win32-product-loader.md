# Win32 제품 loader 통합 작업 로그

## 결과

일반 Win32 제품 실행 경로를 `re2dj.exe --hdd <dir> --target ez2dj1stse --run`으로 연결했다. Windows loader가 `ez2dj.exe`를 원본 process의 주 image로 적재하고 기존 injected runtime이 Win32/DirectX 경계를 HLE하는 방식은 유지했다. 따라서 게임 로직을 다시 구현하거나 제품 경로를 custom PE manual mapper로 교체하지 않았다.

기존 launcher core는 `re2dj_windows_original_process_backend` static library로 추출했다. 제품 CLI는 typed facade를 통해 검증된 canonical 옵션을 구성하고, `re2dj_windows_x86_launcher_probe`는 같은 engine을 호출하는 얇은 진단 entry로 남겼다. 현재 제품 정책은 검증된 `ez2dj1stse`만 허용하며 다른 target은 process 생성 전에 거부한다.

## 변경 범위

- Windows original-process backend와 제품 facade 추가
- 일반 Win32 `--run` 연결 및 도움말/오류 계약 갱신
- launcher probe를 공용 engine 기반의 얇은 entry로 변경
- canonical argument와 unsupported-target을 검사하는 asset-free probe 추가
- README, architecture, 실행 guide, 구현 현황 문서 갱신

## 검증

- `cmake --build --preset windows-x86-debug --config Debug`: 성공
- `ctest --preset windows-x86-debug --output-on-failure`: 3/3 성공
- `re2dj_windows_product_loader_probe.exe`: canonical policy와 unsupported target 검사 성공
- `re2dj.exe --help`: Windows `--run` 지원 target과 overlay 정책 표시 확인
- `re2dj_windows_x86_launcher_probe.exe --help`: 기존 진단 옵션 표시 확인
- Linux headless `re2dj` 증분 build와 CTest: 1/1 성공

실제 원본 실행 검증은 저장소에 원본 자산을 두지 않는 정책에 따라 수행하지 않았다. 사용자가 제공한 HDD 디렉터리에서 제품 명령과 기존 canonical 진단 명령의 화면, 로그, 종료 동작을 비교하는 후속 검증이 필요하다.

---

# Win32 Product Loader Integration Work Log

## Result

The ordinary Win32 product path is now connected through `re2dj.exe --hdd <dir> --target ez2dj1stse --run`. The verified execution model remains unchanged: the Windows loader maps `ez2dj.exe` as the original process's main image, and the existing injected runtime HLEs the Win32/DirectX boundaries. This neither reimplements game logic nor replaces the product path with a custom PE manual mapper.

The launcher core was extracted into the `re2dj_windows_original_process_backend` static library. The product CLI constructs the verified canonical options through a typed facade, while `re2dj_windows_x86_launcher_probe` remains a thin diagnostic entry calling the same engine. The product policy currently permits only the verified `ez2dj1stse` target and rejects other targets before process creation.

## Change Scope

- Added the Windows original-process backend and product facade.
- Connected ordinary Win32 `--run` and updated its help and error contract.
- Converted the launcher probe into a thin entry over the shared engine.
- Added an asset-free probe for canonical arguments and unsupported targets.
- Updated the README, architecture, execution guide, and implementation status.

## Verification

- `cmake --build --preset windows-x86-debug --config Debug`: passed.
- `ctest --preset windows-x86-debug --output-on-failure`: 3/3 passed.
- `re2dj_windows_product_loader_probe.exe`: canonical policy and unsupported-target checks passed.
- `re2dj.exe --help`: confirmed Windows `--run` target support and overlay policy.
- `re2dj_windows_x86_launcher_probe.exe --help`: confirmed existing diagnostic options remain available.
- Linux headless incremental `re2dj` build and CTest: 1/1 passed.

An actual original-binary run was not performed because original assets are never placed in the repository. Follow-up validation should compare display, logs, and exit behavior between the product command and the existing canonical diagnostic command using a user-supplied HDD directory.
