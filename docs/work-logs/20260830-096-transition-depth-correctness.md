# 작업 로그: 장면 전환과 깊이 정렬 보정

## 결과 요약

작업 096을 구현했다. SDL3/OpenGL backend는 16-bit depth buffer를 요청하고, frame 첫 draw에서 color/depth를 함께 clear하며, draw마다 Direct3D depth test/write/compare 상태를 적용한다. Windows facade는 Direct3D 비교 함수와 `D3DBLEND_INVSRCOLOR`/`D3DBLEND_INVSRCALPHA`를 매핑한다.

초기 transition에서 관찰된 검정 full-screen alpha quad에는 제한된 호환성 보정을 적용했다. texture가 없고 네 정점이 논리 화면 전체를 덮으며 RGB가 검정이고 alpha가 동일한 경우에만 source-alpha/inverse-source-alpha blending을 사용한다. `LateDraw`에 guest state와 effective state 및 `fadecompat` marker를 함께 기록한다.

## 변경 사항

- `LegacyFixedFunctionState`에 depth test/write/compare와 fade marker 추가.
- `BlendFactor::kInverseSourceAlpha`, 전체 Direct3D compare enum 추가.
- SDL GL context depth size 16 요청, `glDepthFunc`/`glDepthMask` 로드, color+depth clear, depth state 적용.
- Direct3D `ZENABLE`, `ZWRITEENABLE`, `ZFUNC` 매핑과 `D3DBLEND_INVSRCALPHA` 지원.
- `D3DBLEND_INVSRCOLOR` 지원. 최종 실행에서 기존 미지원 경로로 64회 실패하던 draw가 `GL_ONE_MINUS_SRC_COLOR`로 전달된다.
- 무텍스처 검정 full-screen uniform-alpha quad의 좁은 fade compatibility 정책 추가.
- runtime probe에 depth와 inverse-alpha 상태 설정·draw·복원 검증 추가.
- 설계, 작업 지시서, 분석 문서, 분석 색인, TODO, Implemented 갱신.

## 검증 증거

1. Windows x86 Debug 전체 빌드: `cmake --build build\\windows-x86 --config Debug -- /m:1 /v:minimal` 성공.
2. Windows x86 Debug CTest: 3/3 통과.
3. Windows x86 Release 전체 빌드: `cmake --build build\\windows-x86 --config Release -- /m:1 /v:minimal` 성공.
4. Windows x86 Release CTest: 3/3 통과.
5. 실제 실행 `20260830-104631-572.ddraw.log`:
   - 초기 transition draw에서 `texture=0`, `bounds=0,0,640,480`, `diffuse=0x0c000000` 등을 확인.
   - guest `blend=0`인 동일 draw가 effective source-over와 `fadecompat=1`로 기록됨(`SRCALPHA`/`INVSRCALPHA`).
   - 전체 관찰 draw에서 `zenable=0`, `zwrite=0`, `zfunc=0`; Z-enabled 원본 장면은 확인되지 않음.
   - 수백 회 draw/present 동안 graphics failure가 기록되지 않음.
   - 후속 최종 실행 `20260830-105546-182`에서 기존 `dstblend=4`(`INVSRCOLOR`) draw 거절 64회를 확인해 추가 매핑 근거를 확보함.
   - 수정 후 실행 `20260830-110037-388`은 `fadecompat=1` 41회와 `LateDraw` 512회를 기록했고 `unsupported Direct3D3 alpha blend factor`는 0회였다.
6. Linux 공용 backend build는 기존 `build/linux-x64-debug` cache가 WSL 경로(`/mnt/e/...`)로 생성되어 Windows 경로와 달라 중단됐다. 소스 오류가 아니라 재구성되지 않은 플랫폼별 build 디렉터리 문제이며, 이번 Windows 작업의 실패 근거로 사용하지 않는다.

실행은 detached 정책으로 child가 먼저 종료된 뒤 launcher 세션이 남았고, 세션은 검증 후 Ctrl+C로 정리했다. 이 때문에 이번 로그는 정상 창 닫기 exit-code 증거가 아니라 렌더러 상태·진단 marker 증거로만 사용한다. 사용자 화면에서 모든 전환이 개선되었는지는 TODO로 남겼다.

## 판단과 후속 작업

깊이 경계 누락은 일반적으로 수정했고, `INVSRCOLOR` draw 거절도 제거했다. 현재 canonical trace가 Z를 켜지 않으므로 실제 z 정렬 개선은 미확정이다. fade 보정은 원본 driver 의미를 확정한 것이 아니라 trace와 증상에 근거한 호환성 정책이므로 사용자 재검증 후 유지 여부를 결정한다.

---

# Work log: scene-transition and depth-ordering correction

## Result

Task 096 is implemented. The SDL3/OpenGL backend requests a 16-bit depth buffer, clears color and depth on the first draw of each frame, and applies Direct3D depth test/write/compare state per draw. The Windows facade maps Direct3D comparison values and `D3DBLEND_INVSRCOLOR`/`D3DBLEND_INVSRCALPHA`.

A narrow compatibility rule handles the black full-screen vertex-alpha quad observed during early transitions. It requires no texture, a four-vertex logical-screen triangle strip, uniform black RGB, and intermediate uniform alpha. `LateDraw` records guest state, effective state, and the `fadecompat` marker separately.

## Verification

- Windows x86 Debug build succeeded; Debug CTest passed 3/3.
- Windows x86 Release build succeeded; Release CTest passed 3/3.
- Product trace `20260830-104631-572.ddraw.log` records the expected full-screen black transition quads and `fadecompat=1` with effective `SRCALPHA`/`INVSRCALPHA` while guest `blend=0`.
- Every observed product draw still has guest `zenable=0`, `zwrite=0`, and `zfunc=0`, so an original Z-enabled scene is unresolved.
- Hundreds of draws/presents complete without graphics failure.
- Final run `20260830-105546-182` exposed 64 rejected `dstblend=4` (`INVSRCOLOR`) draws in the old mapping; the new `GL_ONE_MINUS_SRC_COLOR` mapping forwards them.
- Corrected run `20260830-110037-388` records 41 fade markers and 512 `LateDraw` entries with zero unsupported-blend failures.
- The existing `build/linux-x64-debug` cache was created under a WSL path (`/mnt/e/...`) and cannot be reused from the Windows path, so the Linux build stopped before compilation. This is a stale, platform-specific build-directory issue rather than a source failure and is not used as evidence against this Windows task.

The run used the detached policy: the child became unavailable before the launcher session was manually interrupted with Ctrl+C. Therefore this run is evidence for renderer state and diagnostics, not for a normal close exit code. User-visible correction across all transitions remains a TODO.

## Follow-up

The generic depth boundary is now present, but product z-order improvement needs a trace or visual case with Z enabled. The fade rule is an inferred compatibility policy rather than a confirmed original-driver contract and should be retained or removed after user revalidation.
