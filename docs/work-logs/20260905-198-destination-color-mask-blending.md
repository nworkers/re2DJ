# 작업 로그 198: 목적지 색상 마스크 합성
# Work Log 198: Destination-Color Mask Composition

설계: [목적지 색상 블렌드](../design/20260905-198-destination-color-mask-blending.md), 작업 지시: [198](../work-orders/20260905-198-destination-color-mask-blending.md)

*Design: [Destination-color blending](../design/20260905-198-destination-color-mask-blending.md); work order: [198](../work-orders/20260905-198-destination-color-mask-blending.md).*

## 발견 / Findings

사용자 실행 `20260905-174233-086`의 frame 1000은 디스크 뒤에 헤더 artwork를 그리지만 ONE/ONE 가산 pass만 성공 로그에 남습니다. 앞선 draw 실패에는 SRCBLEND=9/DESTBLEND=6 미지원이 있으며 초기 texture 73/76의 반복 실패가 전역 64건 상한을 소모합니다. 코드상 미지원 factor는 backend 호출과 `LateDraw` 전에 draw 전체를 거절합니다. 이전 분석은 성공 draw의 상태에 집중하여 이 누락을 놓쳤습니다.

*Frame 1000 of user run `20260905-174233-086` draws the header after the discs, but only its ONE/ONE additive pass appears in successful-draw logs. Earlier failures reject SRCBLEND=9/DESTBLEND=6; textures 73/76 exhaust the global 64-entry failure budget. The unsupported factor rejects the entire draw before backend submission and `LateDraw`. Earlier analysis focused on successful-draw state and missed this gap.*

## 구현 / Implementation

- 공용 `DecodeLegacyBlendFactor`로 기존 변환을 이동하고 DESTCOLOR/INVDESTCOLOR를 추가했습니다. facade와 GPU probe가 같은 변환을 사용합니다.
- OpenGL에 `GL_DST_COLOR`/`GL_ONE_MINUS_DST_COLOR`를 전달했습니다.
- 표면별 첫 draw 실패를 반복 실패 budget 밖에서 기록하고 첫 성공/실패에 frame과 최종 좌표 bounds를 추가했습니다.
- 실제 backend에 합성 장면을 그리는 자산 없는 `re2dj_opengl_blend_probe`를 추가했습니다. GPU 없는 기본 CTest에는 등록하지 않았습니다.
- 분석·KB·EXE 설계·아키텍처를 갱신하고 이전 cull 아키텍처 문단의 깨진 한국어를 복원했습니다.

*Moved blend decoding into shared `DecodeLegacyBlendFactor`, added destination-color/inverse-destination-color support, and mapped them to OpenGL. Per-surface first failures bypass the repeated-failure budget; first success/failure records now include frame and screen bounds. Added an asset-free real-backend composition probe outside default CTest. Updated analysis, KB, executable design, and architecture, including repair of the earlier corrupted Korean culling paragraph.*

## 검증 / Verification

- `cmd /c scripts\build_win32.bat`: Debug build 성공, runtime DLL과 launcher 갱신.
- `re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build/windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 통과.
- `re2dj_opengl_blend_probe.exe`: `pixel checks: 9, failures: 0`. 실제 RGB565 framebuffer readback으로 header 내부 mask 가림, white mask 보존, header 밖 보존, 가산 그림, 부분 source alpha, inverse destination color, 기존 ZERO/SRCCOLOR, Present 이후 복원을 검증했습니다. 양자화 허용 오차는 채널당 8/255입니다.
- `git diff --check`: 통과.

*The Windows Debug build updated the runtime DLL and launcher. Unit tests passed 1265 checks with zero failures; CTest passed 1/1. The actual OpenGL RGB565 probe passed nine pixel checks for mask occlusion, white-mask and out-of-header preservation, additive artwork, partial source alpha, inverse destination color, existing ZERO/SRCCOLOR behavior, and restoration after Present. Per-channel tolerance is 8/255. Whitespace checks passed.*

## 제한과 다음 확인 / Limits and Next Verification

computer-use 스킬의 Windows 창 조회가 `Computer Use native pipe is unavailable` / os error 2로 실패했습니다. 재시도와 JS 세션 초기화 후에도 같아서 UI 입력은 수행하지 못했습니다. 따라서 설계의 수정 전·후 Music Select 자동 화면 비교는 완료되지 않았습니다. 게임 실행을 통한 검증으로 오해하지 않도록 위 9건은 별도 합성 GPU 검사임을 명시합니다.

*Computer-use window discovery failed with `Computer Use native pipe is unavailable` / OS error 2, including retry and JS-session reinitialization. No UI input was performed, so the planned before/after Music Select visual comparison was not completed. The nine checks above are separate synthetic GPU tests, not game-screen verification.*

**미확정:** 헤더 mask의 실제 texture identity/좌표/blend 값, 누락된 목적지 색상 draw 복원이 실제 Music Select 화면을 원본과 일치시키는지 여부. 사용자가 새 build로 재실행하면 첫 성공/실패 기록으로 귀속하고 헤더 뒤 디스크가 가려지는지 비교합니다. 반복 절차는 [블렌드 검증 가이드](../guides/graphics-blend-validation.md)에 둡니다.

*Unresolved: The actual header-mask identity/bounds/blend values and whether restoring destination-color draws matches original Music Select output. Attribute the first success/failure records in the next user run and compare whether the disc is occluded behind the header. Repeatable steps are in the [blend validation guide](../guides/graphics-blend-validation.md).*

## 사용자 검증 결과 / User Verification Result

사용자는 수정본 실행 `20260905-185621-933`에서 화면이 정상으로 돌아왔다고 확인했습니다. 화면에는 원본처럼 `select a music` 헤더 뒤의 작은 디스크와 배경 광선이 가려져 있고, 중앙 artwork의 과도한 밝기도 사라졌습니다.

*The user confirmed that the corrected run `20260905-185621-933` returned the screen to normal. As on the original, the small disc and background rays are hidden behind the `select a music` header, and the center artwork is no longer excessively bright.*

로그는 이 결과를 직접 뒷받침합니다.

*The log directly supports this result.*

- `texture=250`: `srcblend=9`, `dstblend=6`, `reason=success`, frame `1298`, bounds `127,96,383,352` — center mask.
- `texture=280`: `srcblend=9`, `dstblend=6`, `reason=success`, frame `1298`, bounds `-2,-93,642,3` — header mask.
- `texture=281`: `srcblend=2`, `dstblend=2`, `reason=success`, frame `1298` — additive header artwork after the mask.
- `result=0x80004005`, `unsupported Direct3D3 alpha blend factor`, and `draw-failure`: `0`.

*The log records center mask texture 250 and header mask texture 280 as successful DESTCOLOR/INVSRCALPHA draws, followed by successful additive header texture 281. It contains zero generic draw failures, unsupported Direct3D3 blend-factor records, and draw-failure records.*

목적지 색상 blend 미지원으로 mask draw 전체가 생략되었던 것이 이번 출력 차이의 원인으로 확정되었습니다. culling 구현은 실제 상태 차이를 해소했지만 이 증상에는 영향을 주지 않았습니다. Task 198의 원인 분석과 구현 검증이 완료되었습니다.

*The missing destination-color blend support, which dropped the entire mask draw, is confirmed as the cause of this output difference. Culling forwarding resolved a real state gap but did not affect this symptom. Task 198's diagnosis and implementation verification are complete.*
