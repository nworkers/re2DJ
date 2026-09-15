# 작업 288 작업 지시 — present 동기화 정책 / Task 288 work order — Present synchronization policy

설계: [20260915-288-present-sync-policy.md](../design/20260915-288-present-sync-policy.md)

## 한국어

### 범위

present 동기화를 명시적으로 설정하는 기능을 코어부터 제품 명령줄까지 배선한다. 기본값은 현재의 실효 동작(vsync 켜짐)이므로 화면 동작은 바뀌지 않는다.

### 변경 목록

| # | 파일 | 변경 |
| --- | --- | --- |
| 1 | `include/re2dj/graphics/sdl3_opengl_backend.h` | `PresentSync` 열거형과 `Sdl3OpenGlWindowConfig::present_sync` 추가 |
| 2 | `src/graphics/sdl3_opengl_backend.cpp` | `Initialize`에서 interval 설정, 적용값 읽기, trace 한 줄 |
| 3 | `src/platform/windows/graphics_trace_log.h/.cpp` | `g_re2dj_present_sync` 전역과 조회 함수 |
| 4 | `src/platform/windows/direct3d3_com_facade.cpp` | 전역을 `PresentSync`로 변환해 window config에 전달 |
| 5 | `include/re2dj/target/target_profile.h` | `TargetRunDefaults::present_sync` |
| 6 | `include/re2dj/platform/windows/original_process_backend.h`, `src/.../original_process_backend.cpp` | `--present-sync` 인자 생성 |
| 7 | `src/tools/windows_x86_launcher_probe/main.cpp`, `child_process_handoff.*` | `--present-sync` 파싱, 전역 기록, 자식 프로세스 전달 |
| 8 | `src/host/cli/main.cpp` | `--vsync on\|off\|adaptive` 파싱과 `_explicit` 덮어쓰기, 사용법 |
| 9 | `tests/` | 단위 테스트 추가 |
| 10 | `ARCHITECTURE.md`, `README.md`, `docs/IMPLEMENTED.md` | 옵션과 경계 반영 |

### 제약

* 기본값은 `kVerticalSync`. 어떤 프로파일도 이 작업에서 다른 값으로 바꾸지 않는다.
* interval 설정 실패는 초기화 실패가 아니다.
* 적용값은 요청값이 아니라 `SDL_GL_GetSwapInterval` 결과를 기록한다.
* 새 전달 메커니즘을 만들지 않고 기존 내보낸 전역 방식을 재사용한다.
* 게스트 코드와 원본 실행 파일은 건드리지 않는다.

### 검증

1. Windows x86 Debug/Release 빌드.
2. CTest 전체 통과.
3. `re2dj ez2dj3rd`(기본값) 실행 → 타이틀바 FPS가 56~57로 오늘과 같은지, trace에 적용값 `1`이 남는지.
4. `--vsync off`, `--vsync adaptive` 실행 → 적용값과 FPS 기록. 이 수치는 후속 원인 규명의 입력이다.

### 범위에서 뺀 것

* 3rd의 프레임 페이싱 수정. 별도 작업이다.
* Linux·Web 호스트의 정책 배선. 코어 필드는 공용이지만, 두 호스트의 실행 확인은 별도 작업이다.
* 프레임 시간 히스토그램 계측.

## English

### Scope

Wire explicit present synchronization from the shared core through to the product command line. The default is today's effective behavior — vsync on — so nothing changes on screen.

### Change list

| # | File | Change |
| --- | --- | --- |
| 1 | `include/re2dj/graphics/sdl3_opengl_backend.h` | Add the `PresentSync` enumeration and `Sdl3OpenGlWindowConfig::present_sync` |
| 2 | `src/graphics/sdl3_opengl_backend.cpp` | Set the interval in `Initialize`, read back what applied, emit one trace line |
| 3 | `src/platform/windows/graphics_trace_log.h/.cpp` | The `g_re2dj_present_sync` global and its accessor |
| 4 | `src/platform/windows/direct3d3_com_facade.cpp` | Translate the global into `PresentSync` for the window config |
| 5 | `include/re2dj/target/target_profile.h` | `TargetRunDefaults::present_sync` |
| 6 | `include/re2dj/platform/windows/original_process_backend.h`, `src/.../original_process_backend.cpp` | Emit the `--present-sync` argument |
| 7 | `src/tools/windows_x86_launcher_probe/main.cpp`, `child_process_handoff.*` | Parse `--present-sync`, write the global, pass it to a followed child |
| 8 | `src/host/cli/main.cpp` | Parse `--vsync on\|off\|adaptive`, apply the `_explicit` override, update usage |
| 9 | `tests/` | Add unit tests |
| 10 | `ARCHITECTURE.md`, `README.md`, `docs/IMPLEMENTED.md` | Reflect the option and the boundary |

### Constraints

* The default is `kVerticalSync`, and no profile changes it in this task.
* Failing to set the interval is not an initialization failure.
* Record the `SDL_GL_GetSwapInterval` result, not the requested value.
* Reuse the existing exported-global mechanism rather than inventing a new one.
* Do not touch guest code or the original executables.

### Verification

1. Windows x86 Debug and Release builds.
2. Full CTest pass.
3. Run `re2dj ez2dj3rd` with defaults: the title-bar FPS must still read 56-57, and the trace must record an applied interval of `1`.
4. Run `--vsync off` and `--vsync adaptive`, recording the applied interval and the FPS. Those numbers are the input to the follow-up diagnosis.

### Excluded from scope

* Fixing the 3rd frame pacing, which is a separate task.
* Wiring the policy on the Linux and Web hosts. The core field is shared, but confirming those hosts at runtime is separate work.
* Frame-time histogram instrumentation.
