# 작업 429 작업 지시서 — 32비트 트루컬러 표면 선택 옵션 / Task 429 work order — an optional 32-bit true-color surface mode

설계: [20260929-429-true-color-surfaces.md](../design/20260929-429-true-color-surfaces.md)

## 절차 / Steps

1. 공용 `graphics` 코어를 만든다.
   - `color_depth.h`: `ColorDepth`, 이름 해석, 프로세스 스위치.
   - `true_color.h`: 넓힘·줄임, plane, 행 단위 복사·화해·채움.
   - `LegacyTextureView`에 plane 필드를 더한다.

   *Add the shared `graphics` core:*
   - *`color_depth.h`: `ColorDepth`, name parsing, and the process switch;*
   - *`true_color.h`: widening and narrowing, the plane, and row-level copy, reconcile, and fill;*
   - *plane fields on `LegacyTextureView`.*
2. `Sdl3OpenGlBackend`를 바꾼다.
   - 렌더 타깃 형식 전환(`GL_RGB565` ↔ `GL_RGB8`).
   - plane 텍스처 업로드.
   - 32비트 readback·화해 write.
   - `ClearRenderTargetColor`.
   - 렌더 타깃 비트 수 조회.

   *Change `Sdl3OpenGlBackend`:*
   - *render-target format switching (`GL_RGB565` ↔ `GL_RGB8`);*
   - *plane texture upload;*
   - *32-bit readback and reconciling writes;*
   - *`ClearRenderTargetColor`;*
   - *a render-target bit-depth query.*
3. Linux HLE를 바꾼다.
   - `GuestBitmap`의 plane.
   - GDI 네 경로의 동시 쓰기.
   - ddraw 표면의 plane 수명과 쓰기 경로.
   - `HostPresentation::ClearTargetColor`.
   - `LinuxHostPresentation`.

   *Change the Linux HLE:*
   - *the plane on `GuestBitmap`;*
   - *dual writes on the four GDI paths;*
   - *the ddraw surface plane's lifetime and write paths;*
   - *`HostPresentation::ClearTargetColor`;*
   - *`LinuxHostPresentation`.*
4. Windows DX6 COM facade를 바꾼다: 32bpp DIB plane, GetDC/ReleaseDC, Blt, 채움, Clear, Load, Lock/Unlock, 텍스처 뷰.
   *Change the Windows DX6 COM facade: the 32bpp DIB plane, GetDC/ReleaseDC, Blt, fills, Clear, Load, Lock/Unlock, and the texture view.*
5. 옵션을 연결한다.
   - `TargetRunDefaults::color_depth`, 제품 `--color-depth`.
   - Windows: launcher `--color-depth`, `g_re2dj_color_depth` export.
   - Linux: 스위치 설정.

   *Wire the option:*
   - *`TargetRunDefaults::color_depth` and the product's `--color-depth`;*
   - *Windows: the launcher's `--color-depth` and the `g_re2dj_color_depth` export;*
   - *Linux: setting the switch.*
6. OSD를 바꾼다.
   - 공용 색 깊이 토글.
   - Windows 등록.
   - Linux OSD 설치와 입력 연결.

   *Change the OSD:*
   - *a shared colour-depth toggle;*
   - *its Windows registration;*
   - *installing the Linux OSD and routing its input.*
7. 테스트를 만든다: 코어·gdi raster·ddraw 모듈 단위 테스트와 `re2dj_opengl_true_color_probe`.
   *Add the tests: core, gdi-raster, and ddraw-module unit tests, and `re2dj_opengl_true_color_probe`.*
8. 빌드·테스트·실행 검증 뒤 누적 문서를 갱신하고, 작업 로그를 쓴다.
   - 누적 문서: `ARCHITECTURE.md`, `docs/analysis`, `docs/kb`, README, 가이드.

   *After build, test, and run verification, update the cumulative documents and write the work log.*
   - *Cumulative documents: `ARCHITECTURE.md`, `docs/analysis`, `docs/kb`, the README, and the guides.*

## 완료 조건 / Done when

- Windows x86과 Linux x86·x64의 build와 모든 테스트가 통과한다.
  *Windows x86 and Linux x86 and x64 build and pass every test.*
- 두 probe가 실제 GL에서 통과한다.
  *Both probes pass on real GL.*
- Linux 1st SE가 두 모드와 OSD 전환에서 오류 없이 돌고, 캡처에서 32비트 모드의 색 차이가 보인다.
  *Linux 1st SE runs without errors in both modes and across OSD switches, and captures show the 32-bit mode's colour difference.*
