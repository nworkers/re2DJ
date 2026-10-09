# #6 작업 로그 — OSD에 OpenGL renderer 표시 / #6 work log — the OpenGL renderer in the OSD

이슈: [#6](https://github.com/reexec/re2DJ/issues/6) · 설계: [20261006-i006-osd-gl-renderer.md](../design/20261006-i006-osd-gl-renderer.md) · 지시서: [20261006-i006-osd-gl-renderer.md](../work-orders/20261006-i006-osd-gl-renderer.md)

## 2026-10-06

- **출발점**: 사용자가 rePIU OSD(rePIU #5)처럼 GL renderer를 프로세스 정보 아래에 표시해 달라고 요청했다. rePIU의 `GlRendererIdentity`·`IsSoftwareGlRenderer`·`DrawRendererSection`을 re2DJ 구조에 맞춰 옮겼다. rePIU의 `wsl_d3d12` 플래그는 re2DJ에 해당 드라이버 선택이 없어 뺐다.
- **구현**
  - `include/re2dj/graphics/gl_renderer_identity.h`, `src/graphics/gl_renderer_identity.cpp`(신규, `re2dj_legacy_graphics`): 구조체, 소프트웨어 판정, `unknown` 대체.
  - `Sdl3OpenGlBackend`: context 생성 직후 값을 읽어 `renderer_identity()`로 내준다. 처음엔 `glGetString`을 직접 불러 Windows x86 링크가 `__imp__glGetString@4`에서 실패했다. 이 backend는 opengl32를 링크하지 않고 GL 진입점을 모두 `SDL_GL_GetProcAddress`로 얻으므로 `glGetString`도 `LoadGlFunction`으로 바꿨다.
  - `ui::Osd::SetRendererIdentity`와 정보 줄 아래 Renderer 절. 경고 색은 셰이더 오류 줄과 같은 값이라 `kWarningColor` 상수로 묶었다.
  - `SdlHostPresentation::ReportRenderer`: OSD에 넘기고 `presentation: GL renderer: … | vendor: … | version: … | video driver: …` 로그.
  - 단위 테스트 `tests/unit/gl_renderer_identity_test.cpp`, `ARCHITECTURE.md` 절 추가.

  *Starting point: the user asked for the GL renderer below the OSD's process information, as in rePIU's OSD (rePIU #5). rePIU's `GlRendererIdentity`, `IsSoftwareGlRenderer` and `DrawRendererSection` were carried over to re2DJ's layout, without rePIU's `wsl_d3d12` flag since re2DJ makes no such driver choice. Implementation: the new `gl_renderer_identity.{h,cpp}` in `re2dj_legacy_graphics` (structure, software test, `unknown` substitution); `Sdl3OpenGlBackend` reads the values right after creating the context and exposes `renderer_identity()` — a direct `glGetString` call first failed to link on Windows x86 (`__imp__glGetString@4`), since this backend links no opengl32 and gets every GL entry point through `SDL_GL_GetProcAddress`, so it now goes through `LoadGlFunction` too; `ui::Osd::SetRendererIdentity` and the Renderer section below the information lines, with the warning colour shared with the shader error line as `kWarningColor`; `SdlHostPresentation::ReportRenderer` hands it to the OSD and logs it; the unit test `tests/unit/gl_renderer_identity_test.cpp` and an `ARCHITECTURE.md` section.*

- **검증**
  - Windows x86 Debug: `re2dj`, `re2dj_unit_tests` 빌드. 단위 테스트 checks 6158, failures 0.
  - 실제 실행(`--hdd roms/ez2dj6th --target ez2dj6th --run --windowed`): 로그에 `presentation: GL renderer: NVIDIA GeForce RTX 4090/PCIe/SSE2 | vendor: NVIDIA Corporation | version: 4.6.0 NVIDIA 616.56 | video driver: windows`. 백틱으로 연 OSD 캡처에서 Executable 줄 아래 Renderer 구분선, renderer, Vendor, OpenGL, Video driver 줄을 확인했다. 이 장비는 GPU라 경고 표시는 실행으로 보지 못했고 단위 테스트로만 확인했다.
  - Linux x64 Debug(WSL): `re2dj`, `re2dj_unit_tests` 빌드, 경고 없음. 단위 테스트 checks 6155, failures 0. Linux x86과 clang 타깃은 작업 브랜치 push 뒤 CI로 확인한다.

  *Verification: on Windows x86 Debug, `re2dj` and `re2dj_unit_tests` build and the unit tests report 6158 checks, 0 failures. A real run (`--hdd roms/ez2dj6th --target ez2dj6th --run --windowed`) logs `presentation: GL renderer: NVIDIA GeForce RTX 4090/PCIe/SSE2 | vendor: NVIDIA Corporation | version: 4.6.0 NVIDIA 616.56 | video driver: windows`, and a capture of the OSD opened with backtick shows the Renderer separator and the renderer, Vendor, OpenGL and Video driver lines below the Executable line. This machine has a GPU, so the software warning was checked by the unit tests only. On Linux x64 Debug (WSL) both build without warnings and the unit tests report 6155 checks, 0 failures; Linux x86 and the clang target are checked by CI once the task branch is pushed.*
