# 작업 308 — WSL Linux 기준선 결과 / Task 308 — WSL Linux Baseline Results

## 결과 / Results

L0 기준선을 재현했다. 계획 commit `7445a49`에서 시작해 GCC portability 문제만 수정했다. FAT32 이름 처리와 기존 fixture의 structured binding을 const reference로 바꾸고, CLI의 Windows 전용 함수 두 개에 호출부와 같은 `_WIN32` 조건을 적용했다. Hardlock 테스트에는 누락된 `<algorithm>`, `<array>`, `<cstring>`을 직접 포함했다. 새 HLE나 x86 제품 지원은 구현하지 않았다.

*Reproduced L0 from planning commit 7445a49 with compiler-portability fixes only: const-reference structured bindings in FAT32 name handling and its existing fixture, matching _WIN32 guards for two Windows-only CLI helpers, and explicit algorithm/array/cstring test headers. No new HLE or x86 product support was implemented.*

| 검증 / Check | 결과 / Result |
| --- | --- |
| Linux x64 Debug 전체 build / Full build | GCC 13.3.0, warnings-as-errors 통과 / Passed |
| i386 helper Debug build | `-m32`, warnings-as-errors 통과 / Passed |
| Linux CTest | 1/1 통과 / Passed |
| 실행 파일 폭 / Executable classes | 제품 ELF64, helper ELF32 / Product ELF64, helper ELF32 |
| synthetic native IPC | result 51, child 0 |
| synthetic fault | SIGILL 4, EIP `0x1000100f`, nonzero guest ESP |
| OpenGL blend probe | 10 checks, 0 failures |
| Windows x86 Debug | `re2dj`·`re2dj_unit_tests` build 성공, 선택 CTest 1/1 / Selected builds and CTest passed |

환경은 Ubuntu 24.04.1 / WSL2 kernel `5.15.167.4-microsoft-standard-WSL2`, CMake 3.28.3, Ninja 1.11.1이다. multilib·i386 loader와 x86/x64 multimedia 개발 패키지가 이미 있었다. Clang은 PATH에서 발견되지 않았다. WSLg 1.0.65의 X11/Wayland/Pulse 환경 변수는 존재하지만 실제 검증은 OpenGL까지다. Mesa 25.2.8 llvmpipe로 실행되어 GPU 가속은 확인되지 않았다.

*Environment: Ubuntu 24.04.1, WSL2 kernel 5.15.167.4-microsoft-standard-WSL2, CMake 3.28.3, Ninja 1.11.1. Multilib, the i386 loader, and x86/x64 multimedia development packages were already installed. Clang was absent from PATH. WSLg 1.0.65 exposed X11/Wayland/Pulse variables, but only OpenGL was exercised. Rendering used Mesa 25.2.8 llvmpipe, without GPU acceleration.*

## 재현 명령과 산출물 / Reproduction and artifacts

WSL Linux filesystem의 `/home/nworkers/re2dj-builds/20260918-308` 아래 x64/helper tree와 configure/build/CTest 로그를 유지했다. 최초 `/tmp` tree는 후속 호출 시 사라진 것을 확인해 옮겼으며 원인은 확정하지 않았다. 패키지를 새로 설치하지 않았다. SDL cache는 지정된 revision `147a8ee32dbf9ac02f3794964490687b6bbda1bc`이며 Windows 파일 모드·줄바꿈 차이를 제외한 diff가 없었다. ImGui는 CMake가 고정 revision `01380c579715e62fb9a8d6ec0502c4ea83bfde6e`를 가져왔다.

*Retained x64/helper trees and configure/build/CTest logs under /home/nworkers/re2dj-builds/20260918-308. The initial /tmp tree was absent on a later invocation; the cause is unresolved. No packages were installed. The SDL cache matched pinned revision 147a8ee32dbf9ac02f3794964490687b6bbda1bc with no diff after accounting for Windows file modes/line endings. CMake fetched ImGui at pinned revision 01380c579715e62fb9a8d6ec0502c4ea83bfde6e.*

다음 명령은 이 작업의 실제 경로를 사용한 재현 기록이다. 일반 사용자 절차는 기존 [빌드 가이드](../guides/linux-sdl3-build.md)를 참조한다.

*These commands record this task's actual paths. See the existing [build guide](../guides/linux-sdl3-build.md) for the general procedure.*

```bash
cmake -S /mnt/e/MYWORK/Projects/re2DJ -B /home/nworkers/re2dj-builds/20260918-308/x64 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRE2DJ_WARNINGS_AS_ERRORS=ON -DFETCHCONTENT_SOURCE_DIR_SDL3=/mnt/e/MYWORK/Projects/re2DJ/build/_deps/sdl3-src
cmake --build /home/nworkers/re2dj-builds/20260918-308/x64 -j 6
ctest --test-dir /home/nworkers/re2dj-builds/20260918-308/x64 --output-on-failure
cmake -S /mnt/e/MYWORK/Projects/re2DJ -B /home/nworkers/re2dj-builds/20260918-308/helper -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32 -DRE2DJ_BUILD_TESTS=OFF -DRE2DJ_BUILD_LINUX_NATIVE_HELPER=ON -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build /home/nworkers/re2dj-builds/20260918-308/helper --target re2dj_linux_native_ipc_helper -j 4
timeout 30s /home/nworkers/re2dj-builds/20260918-308/x64/bin/re2dj_linux_native_ipc_host_probe /home/nworkers/re2dj-builds/20260918-308/helper/bin/re2dj_linux_native_ipc_helper
timeout 30s /home/nworkers/re2dj-builds/20260918-308/x64/bin/re2dj_opengl_blend_probe
```

Windows 검증은 `cmake --build build/windows-x86 --config Debug --target re2dj re2dj_unit_tests --parallel 4`와 `ctest --test-dir build/windows-x86 -C Debug -R '^re2dj_unit_tests$' --output-on-failure`로 수행했다. 최초 제한 환경 build는 실패했고 권한 확장 후 성공했다. Web toolchain build는 이번에 수행하지 않았다. 전체 Windows runtime 회귀나 원본 게임 검증으로 확대 해석하지 않는다.

*Windows validation used the build and selected CTest commands above. The initial restricted build failed; an escalated build succeeded. No Web toolchain build was run. These results do not establish full Windows runtime regression coverage or original-game validation.*

## L1 파일 변경 목록 / L1 file-change outline

- `CMakeLists.txt`: Linux backend의 x64 한정 조건과 helper-only 구성의 CLI 링크 의존성을 정리한다. x86 제품은 SDL을 포함하고 helper target은 기존처럼 분리한다.
- `CMakePresets.json`: x86 제품 Debug/Release/test preset과 architecture별 dependency 탐색을 추가한다.
- `src/platform/linux/original_runner.cpp`, 합성 integration test: 파일 읽기 완료 판정과 실제 CLI/runner 첫 경계를 검증한다. 기존 host probe 성공만으로 제품 runner 성공을 가정하지 않는다.
- `scripts/test_linux_native_helper_probe.sh`, `.github/workflows/ci.yml`: 외부 build 경로, 양쪽 제품 host, helper 통합 검증을 지원한다. Clang 도구 준비 후 GCC/Clang 검증을 확장한다.
- 실행 가이드와 packaging: helper 탐색/staging, host와 helper의 의존성·오류 메시지를 정리한다. audio 활성화는 L6에 남긴다.

*L1 changes target CMake backend/CLI linkage and helper separation; x86 product presets and architecture-specific dependency discovery; synthetic file-read and first-boundary tests for the actual runner; external-build and dual-host helper integration in scripts/CI; and helper staging/dependency diagnostics in documentation/packaging. Prepare Clang before expanding compiler coverage. Linux audio activation remains L6. A successful host probe does not establish that the product runner works.*

누적 확인 상태는 [Linux 실행 기준선 분석](../analysis/linux-runtime-baseline.md)에 기록했다. 원본 HDD나 덤프는 사용하지 않았다.

*Cumulative evidence is recorded in the [Linux baseline analysis](../analysis/linux-runtime-baseline.md). No original HDD or dump was used.*
