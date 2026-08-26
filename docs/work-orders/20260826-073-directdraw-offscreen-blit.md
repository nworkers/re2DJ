# DirectDraw 오프스크린 합성 복구 작업 지시

관련 설계: [DirectDraw 오프스크린 합성 복구](../design/20260826-073-directdraw-offscreen-blit.md)

**구현·자동 검증 완료, 사용자 화면 재검증 대기.** 아래 코드·빌드·실행 항목은 완료됐으며 누락 그림과 컬러키의 최종 육안 확인은 작업 072에 남긴다.

## 작업

1. RGB565 사각형 복사와 inclusive source color-key 계약 및 단위 테스트를 추가한다.
2. `DDSCAPS_OFFSCREENPLAIN` surface 생성 경로를 기존 GDI backing 정책에 연결한다.
3. `IDirectDrawSurface4::BltFast`를 연결하고 `Blt` source-copy 경로를 구현한다.
4. surface revision과 수명 관리를 기존 texture/primary surface와 동일하게 유지한다.
5. x86/x64 빌드, CTest, detached 원본 실행으로 회귀를 검증한다.
6. 분석, 아키텍처, TODO, IMPLEMENTED와 작업 로그를 실제 결과에 맞게 갱신하고 커밋한다.

---

# DirectDraw Offscreen Composition Recovery Work Order

Related design: [DirectDraw Offscreen Composition Recovery](../design/20260826-073-directdraw-offscreen-blit.md)

**Implementation and automated verification complete; user-visible revalidation pending.** The code, build, and runtime items below are complete. Final visual confirmation of missing sprites and color-key output remains under Task 072.

Add and test a platform-neutral RGB565 rectangle-copy contract, create GDI-backed `DDSCAPS_OFFSCREENPLAIN` surfaces, connect `IDirectDrawSurface4::BltFast`, extend `Blt` with source copying, preserve surface revision/lifetime behavior, verify x86/x64 builds and CTest, repeat the detached original runtime, then update the evidence documents and commit the task.
