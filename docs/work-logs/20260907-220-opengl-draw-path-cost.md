# 20260907-220 OpenGL draw 경계 고정 비용 제거 작업 로그 / OpenGL Draw Boundary Fixed-Cost Work Log

* 설계: [20260907-219-runtime-performance-hot-paths.md](../design/20260907-219-runtime-performance-hot-paths.md)
* 작업 지시: [20260907-220-opengl-draw-path-cost.md](../work-orders/20260907-220-opengl-draw-path-cost.md)

## 변경 내용 / What Changed

`src/graphics/sdl3_opengl_backend.cpp`와 `include/re2dj/graphics/sdl3_opengl_backend.h`만 바뀌었다.

* `Impl::UniformLocations`를 추가하고 프로그램 링크 직후 6개 uniform 위치를 한 번 조회한다. `Draw`와 `Present`가 그 값을 쓴다. draw마다 있던 `glGetUniformLocation` 5회 문자열 조회가 사라졌다.
* `Impl`이 `vertex_scratch`를 소유한다. `Draw`는 draw마다 `clear()` 후 재사용하므로 정점 변환 단계의 힙 할당이 사라졌다.
* `Impl::MakeCurrent`가 `context_current` 플래그를 보고 이미 현재이면 `SDL_GL_MakeCurrent`를 건너뛴다. 이 backend가 프로세스 안 유일한 GL 소비자다.
* 정점 속성 배열 3개를 `Initialize`에서 한 번 활성화한다. `Draw`와 `Present`의 draw별 enable/disable 6회가 사라졌다. 이 프로파일에는 vertex array object가 없으므로 활성 상태는 전역이다.
* `CachedTexture`가 마지막으로 적용한 필터와 주소 모드를 기억한다. 값이 실제로 바뀔 때만 `glTexParameteri`를 호출한다. 같은 샘플러 상태를 반복하는 draw는 파라미터 호출을 하지 않는다.
* `Draw`의 `glGetError`는 초기 256 draw까지, 그리고 진단이 켜졌을 때만 수행한다. `Present`의 프레임당 검사는 조건 없이 그대로 둔다.
* `Sdl3OpenGlWindowConfig`에 `draw_diagnostics`를 추가했다. 호스트가 정하고 `src/graphics/`는 플랫폼 공용으로 남는다. `direct3d3_com_facade.cpp`가 backend 초기화 시 이 값을 채운다.

*Only the SDL3 OpenGL backend source and header changed: uniform locations are resolved once after link and reused, a scratch vertex vector on `Impl` removes the per-draw allocation, `MakeCurrent` short-circuits when the context is already current, the three vertex attribute arrays are enabled once at initialization instead of toggled per draw, `CachedTexture` remembers the applied filter and address mode so `glTexParameteri` fires only on an actual change, and the per-draw `glGetError` is limited to the first 256 draws or to diagnostic runs while `Present` keeps its unconditional per-frame check. A `draw_diagnostics` field on the window config carries the host decision without making the shared graphics layer platform-aware.*

## draw 1회당 제거된 호출 / Calls Removed Per Draw

| 항목 | 변경 전 | 변경 후 |
| --- | --- | --- |
| `SDL_GL_MakeCurrent` | 1 | 0 (첫 회 이후) |
| `glGetUniformLocation` | 5 | 0 |
| `glEnableVertexAttribArray` / `glDisableVertexAttribArray` | 6 | 0 |
| `glTexParameteri` | 4 | 0 (샘플러 상태가 같을 때) |
| `glGetError` | 1 | 0 (초기 256 draw 이후, 진단 꺼짐) |
| `std::vector<GlVertex>` 힙 할당 | 1 | 0 |

## 검증 / Verification

* Windows x86 전체 빌드 성공. 새 경고 없음.
* `re2dj_unit_tests`: checks 1404, failures 0.
* `re2dj ez2dj4th`로 실제 4th CHD를 실행해 그래픽 초기화와 렌더링까지 도달하는 것을 확인했다. `.ddraw.log`에 `CreateSurface`, `RenderState`, `Flip`, `LegacyDeviceClear`, `DrawPrimitive` 항목이 정상적으로 남았고 OpenGL 실패 항목은 없다.

*The full Windows x86 build succeeds with no new warnings, the unit suite passes, and running the real 4th CHD through `re2dj ez2dj4th` reaches graphics initialization and rendering with the expected surface, render-state, flip, clear, and draw records in the `.ddraw.log` and no OpenGL failure entries.*

## 한계 / Limits

* 프레임 레이트를 계측하지 않았다. 이번 변경의 효과를 수치로 제시하지 않는다. 제거한 호출 수만 확인된 사실이다.
* `context_current` 플래그는 이 backend가 유일한 GL 소비자라는 전제에 기댄다. 다른 컨텍스트를 현재로 만드는 경로가 생기면 이 전제를 다시 확인해야 한다.
* `glGetError`를 draw마다 부르지 않게 되어, 특정 draw 하나만 실패하는 경우의 검출이 초기 256 draw 이후로는 프레임 단위 검사로 미뤄진다. `--graphics-draw-diagnostics`로 원래 동작을 복구할 수 있다.

*Frame rate was not measured, so no numeric claim is made; only the removed call counts are confirmed. The `context_current` flag assumes this backend is the only GL consumer, which must be revisited if another context is ever made current. Detection of a single failing draw after the first 256 now waits for the per-frame check, and the diagnostic option restores the original behavior.*
