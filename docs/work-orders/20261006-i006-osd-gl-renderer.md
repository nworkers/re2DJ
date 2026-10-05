# #6 작업 지시서 — OSD에 OpenGL renderer 표시 / #6 work order — the OpenGL renderer in the OSD

이슈: [#6](https://github.com/nworkers/re2DJ/issues/6) · 설계: [20261006-i006-osd-gl-renderer.md](../design/20261006-i006-osd-gl-renderer.md)

## 절차 / Steps

1. `gl_renderer_identity.{h,cpp}`: 구조체, `IsSoftwareGlRenderer`, `MakeGlRendererIdentity`. `re2dj_legacy_graphics`에 추가.
   *Add the structure, `IsSoftwareGlRenderer` and `MakeGlRendererIdentity` to `re2dj_legacy_graphics`.*
2. `Sdl3OpenGlBackend`: context 생성 직후 구조체를 채우고 `renderer_identity()`로 내준다.
   *Fill it right after the context is created and expose it as `renderer_identity()`.*
3. `ui::Osd::SetRendererIdentity`와 정보 줄 아래 Renderer 절.
   *`ui::Osd::SetRendererIdentity` and the Renderer section below the information lines.*
4. `SdlHostPresentation`: OSD에 넘기고 로그 한 줄.
   *Hand it to the OSD and log one line.*
5. 단위 테스트 `gl_renderer_identity_test.cpp`.
   *Unit test `gl_renderer_identity_test.cpp`.*
6. `ARCHITECTURE.md` OSD 설명 갱신, 설계의 검증 실행.
   *Update the OSD description in `ARCHITECTURE.md` and run the design's verification.*

## 완료 조건 / Done when

OSD를 열면 프로세스 정보 아래에 renderer·vendor·OpenGL 버전·비디오 드라이버가 보이고, 로그에 같은 값이 남으며, 단위 테스트가 통과한다.

*Opening the OSD shows the renderer, vendor, OpenGL version and video driver below the process information, the log holds the same values, and the unit tests pass.*
