# 20260907-220 OpenGL draw 경계 고정 비용 제거 계획 / OpenGL Draw Boundary Fixed-Cost Work Order

설계: [20260907-219-runtime-performance-hot-paths.md](../design/20260907-219-runtime-performance-hot-paths.md)

## 목표 / Goal

`Sdl3OpenGlBackend::Draw`의 draw 1회당 고정 비용을 제거한다. 화면에 그려지는 결과는 변하지 않아야 한다.

*Remove the per-draw fixed cost in `Sdl3OpenGlBackend::Draw` without changing what is rendered.*

## 작업 항목 / Work Items

1. `Impl`에 uniform location 필드를 추가하고 프로그램 링크 직후 한 번 조회한다. `Draw`와 `Present`가 그 값을 쓴다.
2. `Impl`이 `std::vector<GlVertex>` 스크래치를 소유하고, `Draw`가 draw마다 `clear()` 후 재사용한다.
3. `Impl`에 컨텍스트 현재화 플래그를 두고, 이미 현재이면 `SDL_GL_MakeCurrent`를 건너뛴다.
4. 정점 속성 배열을 초기화 시 한 번 활성화하고, `Draw`와 `Present`의 draw별 활성/비활성 호출을 제거한다.
5. `CachedTexture`에 마지막으로 설정한 필터와 주소 모드를 저장하고, 값이 바뀔 때만 `glTexParameteri`를 호출한다.
6. `Sdl3OpenGlWindowConfig`에 진단 플래그를 추가한다. `Draw`의 `glGetError`는 초기 일정 draw 수까지, 그리고 진단이 켜졌을 때만 수행한다. `Present`의 검사는 그대로 둔다.
7. 호출부인 `direct3d3_com_facade.cpp`가 backend 초기화 시 진단 플래그를 채운다.

*Cache uniform locations in `Impl` after link and use them in `Draw` and `Present`; own a reusable `GlVertex` scratch vector; skip `SDL_GL_MakeCurrent` when the context is already current; enable the vertex attribute arrays once at initialization and drop the per-draw toggles; store the last-applied filter and address mode on `CachedTexture` and call `glTexParameteri` only on change; add a diagnostic flag to `Sdl3OpenGlWindowConfig` that, together with an initial draw budget, decides whether `Draw` checks `glGetError`, while `Present` keeps checking; fill that flag from `direct3d3_com_facade.cpp`.*

## 제약 / Constraints

* `src/graphics/`는 플랫폼 공용이다. Windows 전용 상태를 직접 참조하지 않는다. 진단 여부는 설정 구조체로 전달받는다.
* 렌더 상태의 의미를 바꾸지 않는다. 생략은 "이미 같은 값"인 경우로만 한정한다.
* 소스 주석은 영어로만 작성한다.

*`src/graphics/` stays platform-neutral and must not reference Windows-only state; the diagnostic decision arrives through the config struct. Render-state meaning is unchanged, and calls are skipped only when the value is already identical. Source comments are English only.*

## 검증 / Verification

* Windows x86 빌드 성공.
* Linux SDL3 backend 대상이 구성된 환경에서도 컴파일이 깨지지 않아야 한다.
* `re2dj_unit_tests` 전체 통과.
* 실행 관찰로 화면 출력이 이전과 동일한지 확인하고 결과를 작업 로그에 남긴다.

*Build Windows x86 successfully, keep the Linux SDL3 backend target compiling, pass the full unit tests, and confirm by runtime observation that the rendered output is unchanged, recording the result in the work log.*
