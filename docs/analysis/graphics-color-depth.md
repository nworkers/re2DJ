# 표시 색 깊이 / Display colour depth

관련 설계: [작업 429 — 32비트 트루컬러 표면 선택 옵션](../design/20260929-429-true-color-surfaces.md)

*Related design: [Task 429 — an optional 32-bit true-color surface mode](../design/20260929-429-true-color-surfaces.md)*

## 원본이 요구하는 표시 모드 / The display mode the originals ask for

- **확인됨.** 1st SE는 `IDirectDraw4::SetDisplayMode(640, 480, 16, 0, 0)`을 한 번 부른다. 복귀 주소는 `0x0041f82e`다. 4th는 `IDirectDraw4::SetDisplayMode(640, 480, 16, 60, 0)`을 부르며 복귀 주소는 `0x00410a72`다. 근거는 Linux 실행 API 로그 `20260928-141813-648`, `20260928-140720-494`다.
- **확인됨.** 1st SE 실행(`20260928-141813-648`, 약 800프레임)에는 `Lock`/`Unlock`/`GetSurfaceDesc`/`GetPixelFormat` 호출이 하나도 없다. 게임은 표면 픽셀을 `GetDC`(42회), `Blt`(1,004회), `DrawPrimitive`(4,195회), `Flip`으로만 다룬다. 따라서 1st SE의 게임 코드는 표면 메모리의 픽셀 형식을 직접 해석하지 않는다.
- **확인됨.** 4th는 표면 DC에 `StretchDIBits`를 부른다(`20260928-140720-494`). [작업 424 설계](../design/20260929-424-surface-lock.md)에 따르면 텍스처를 `Lock`해 직접 쓰기도 한다.
- **미확정.** 나머지 타깃(1st, 2nd, 3rd, 5th, 6th, `ez2d2m`)의 요청 깊이. 16비트로 추정하지만, 이 작업에서 실행 로그로 확인하지 않았다.

- ***Confirmed.** 1st SE calls `IDirectDraw4::SetDisplayMode(640, 480, 16, 0, 0)` once, returning to `0x0041f82e`; 4th calls `IDirectDraw4::SetDisplayMode(640, 480, 16, 60, 0)`, returning to `0x00410a72`. Evidence: Linux run API logs `20260928-141813-648` and `20260928-140720-494`.*
- ***Confirmed.** The 1st SE run (`20260928-141813-648`, about 800 frames) makes no `Lock`, `Unlock`, `GetSurfaceDesc`, or `GetPixelFormat` call. The game handles surface pixels only through `GetDC` (42 calls), `Blt` (1,004), `DrawPrimitive` (4,195), and `Flip`, so 1st SE's game code never interprets surface memory's pixel format itself.*
- ***Confirmed.** 4th calls `StretchDIBits` on a surface DC (`20260928-140720-494`) and, per the [Task 424 design](../design/20260929-424-surface-lock.md), also locks textures and writes them directly.*
- ***Unresolved.** The depth the remaining targets (1st, 2nd, 3rd, 5th, 6th, `ez2d2m`) ask for. It is inferred to be 16 bits, but this task did not check it against run logs.*

## 자산의 색 깊이 / The assets' colour depth

- **확인됨.** 1st SE가 `LoadImageA`로 불러 `GetObjectA`로 조회한 BMP는 모두 `BITMAP.bmBitsPixel` 24다. 로그 `20260928-141813-648`에서 `GetObjectA`가 쓴 24바이트 BITMAP 42개의 다섯 번째 DWORD(`bmPlanes`, `bmBitsPixel`)가 모두 `0x00180001`(평면 1, 24비트)이다. 16비트 표시는 이 색을 채널당 5·6·5비트로 깎는다.
- **미확정.** 다른 타깃의 자산 깊이. 4th처럼 게임이 565 픽셀을 직접 쓰는 텍스처는 32비트 모드에서도 색이 그대로다.

- ***Confirmed.** Every BMP 1st SE loads through `LoadImageA` and queries through `GetObjectA` has `BITMAP.bmBitsPixel` 24: in log `20260928-141813-648` the fifth DWORD (`bmPlanes`, `bmBitsPixel`) of all 42 24-byte BITMAPs `GetObjectA` writes is `0x00180001` (one plane, 24 bits). A 16-bit display cuts these colours to 5, 6, and 5 bits per channel.*
- ***Unresolved.** The asset depth of the other targets. Textures whose 565 pixels the game writes itself, as in 4th, keep their colours in 32-bit mode.*

## 32비트 모드 실행 결과 / Runs in 32-bit mode

- **확인됨.** Linux에서 1st SE를 `--color-depth 16`(x86)과 `32`(x86, x64)로 실행해 AMUSE WORLD 로고 장면을 X 서버에서 캡처했다. 16비트는 금속 배경에 가로 띠와 색 얼룩이 있고, 32비트는 부드럽다. 같은 프레임의 같은 300×300 영역에서 고유 색 수는 61개 대 351개다. 두 실행 모두 창이 닫힐 때까지 돌았다. 곧 게스트가 보는 16비트 계약을 바꾸지 않고도 24비트 자산의 색이 화면에 나온다.
- **확인됨.** 4th는 32비트 모드로 90초 동안 오류 없이 돌았다. Windows 1st SE는 그래픽 trace에 32비트 렌더 타깃을 기록했다. 다만 두 실행 모두 화면 비교는 하지 않았다.
- **미확정.** 4th의 `Lock`으로 쓰는 텍스처와 테스트 모드 화면이 32비트 모드에서 어떻게 보이는지. Windows 32비트 모드 화면.

자세한 절차와 수치는 [작업 429 작업 로그](../work-logs/20260929-429-true-color-surfaces.md)에 있다.

- ***Confirmed.** On Linux, 1st SE was run with `--color-depth 16` (x86) and `32` (x86 and x64) and the AMUSE WORLD logo scene captured from the X server. At 16 bits the metal background shows horizontal bands and colour blotches; at 32 bits it is smooth. The same 300×300 area of the same frame holds 61 distinct colours against 351. Both runs lasted until their window closed, so 24-bit asset colours reach the screen without changing the 16-bit contract the guest sees.*
- ***Confirmed.** 4th ran for 90 seconds in 32-bit mode without errors, and Windows 1st SE recorded a 32-bit render target in its graphics trace; neither run's picture was compared.*
- ***Unresolved.** How 4th's lock-written textures and its test-mode screen look in 32-bit mode, and the Windows picture in 32-bit mode.*

*Procedures and figures are in the [Task 429 work log](../work-logs/20260929-429-true-color-surfaces.md).*
