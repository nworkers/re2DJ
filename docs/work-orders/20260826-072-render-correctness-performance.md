# 렌더링 정확성·성능 회복 작업 지시

관련 설계: [렌더링 정확성·성능 회복](../design/20260826-072-render-correctness-performance.md)

## 상태

**구현·자동 검증 완료, 사용자 화면 재검증 대기.** surface cache, RGB565 color-key alpha, 확인된 fixed-function state와 debugger 분리 실행을 구현했다. 누락 그림·테두리·체감 속도 확인만 남았다.

## 작업

1. texture surface identity/revision과 RGB565 inclusive color-key 범위 계약을 추가하고 단위 테스트한다.
2. OpenGL texture를 surface별로 cache하고 변경된 revision만 upload한다.
3. 성공 draw와 I/O의 고빈도 진단을 기본 실행에서 억제하고 trace 전용 또는 bounded marker로 바꾼다.
4. 원본이 사용하는 render/texture-stage state 조합을 bounded trace로 수집한다.
5. 확인된 color/alpha operation, alpha test와 blending을 공용 state와 OpenGL backend에 반영한다.
6. injected runtime의 target-limited I/O vectored handler와 `--run-detached`를 추가해 debugger exception 왕복을 실제 실행 경로에서 제거한다.
7. x86/x64 build와 CTest, canonical 성능·오류 측정을 수행하고 사용자 시각 검증 command를 남긴다.
8. architecture, TODO, IMPLEMENTED, analysis와 작업 로그를 실제 결과에 맞춰 갱신하고 커밋한다.

## 다음 세션 인계

2026-08-26 종료 시점의 작업 상태는 다음과 같다.

- 현재 브랜치: `render-correctness-performance`
- `main` 기준점: `d1140f4`, tag `v0.0.8`, `VERSION` `0.0.8`
- 구현 커밋: `470ddf0` 렌더링 정확성·detached 성능, `23f4598` DirectDraw offscreen/blit 크래시 복구
- 사용자 상태: 작업 072 수정본의 전체 화면·오디오·입력 검증은 아직 끝나지 않았다. 첫 재검증은 `ez2dj.exe+0x88d6`에서 크래시했으나 작업 073이 원인인 null `1p_meter_back`과 미구현 `DDSCAPS_OFFSCREENPLAIN`/`BltFast`를 복구했다.
- 자동 검증: x86/x64 warnings-as-errors build와 CTest 통과. 수정된 detached 실행 `20260826-201731-528.jsonl`은 기존 약 76초 크래시 지점을 넘어 120초 생존했고 검증을 위해 강제 종료됐다. 같은 시간대 새 WER crash는 없다.

다음 세션은 먼저 `git status`, 현재 브랜치와 이 작업 지시를 확인한 뒤 [Windows x86 원본 실행 가이드](../guides/windows-x86-runtime.md)의 `--run-detached` command를 사용자에게 다시 실행하도록 안내한다. 확인 항목은 누락 그림, 컬러키 테두리, 합성 순서, 체감 속도, `title.wav`와 효과음, 키보드·legacy I/O 입력이다. 사용자가 새 크래시를 보고하면 최신 `logs/windows_x86_launcher_probe/ez2dj1stse/*.jsonl`, Application Error/WER event와 `%LOCALAPPDATA%\CrashDumps\ez2dj*.dmp`를 읽되 로그·dump·원본 자산은 commit하지 않는다.

사용자 검증이 통과하기 전에는 작업 072를 완료 처리하거나 `main`에 머지하지 않는다. 검증이 끝나면 TODO/IMPLEMENTED와 관련 작업 로그를 갱신하고 커밋한 뒤, 사용자 머지 요청에 따라 patch version을 `0.0.9`로 올리고 현재 브랜치 커밋을 하나로 squash하여 `main`에 머지한다. merge commit에는 annotated tag `v0.0.9`를 붙이고 작업 브랜치를 삭제하며, 원격 push는 수행하지 않는다.

---

# Rendering Correctness and Performance Recovery Work Order

Related design: [Rendering Correctness and Performance Recovery](../design/20260826-072-render-correctness-performance.md)

**Implementation and automated verification complete; user-visible revalidation pending.** Surface caching, RGB565 color-key alpha, confirmed fixed-function state, bounded diagnostics, an injected target-limited I/O vectored handler, and `--run-detached` are implemented. x86/x64 builds, CTest, debugger-mode regression, and detached-runtime survival pass. The remaining step is user confirmation of missing images, borders, and perceived speed.

## Next-session handoff

At the end of 2026-08-26, the active branch is `render-correctness-performance`, based on main commit `d1140f4`, tag `v0.0.8`, and `VERSION` `0.0.8`. Implementation commits are `470ddf0` for rendering correctness/detached performance and `23f4598` for DirectDraw offscreen/blit crash recovery. User validation is incomplete. The first revalidation crashed at `ez2dj.exe+0x88d6`; Task 073 recovered the null `1p_meter_back` path and missing `DDSCAPS_OFFSCREENPLAIN`/`BltFast` boundary. x86/x64 warnings-as-errors builds and CTest pass. Detached log `20260826-201731-528.jsonl` survives 120 seconds beyond the former roughly 76-second crash point and is then forcibly stopped, with no new WER crash.

The next session should verify branch/status, read this work order, and ask the user to rerun the `--run-detached` command in the Windows x86 runtime guide. Validate missing sprites, color-key borders, composition order, perceived speed, title/effect audio, and keyboard/legacy-I/O input. For another crash, inspect the latest launcher JSONL, Application Error/WER events, and `%LOCALAPPDATA%\CrashDumps\ez2dj*.dmp`, but never commit logs, dumps, or original assets. Do not complete Task 072 or merge before user confirmation. After successful validation, update TODO/IMPLEMENTED and the work log, commit, bump the patch version to `0.0.9`, squash the branch into `main`, create annotated tag `v0.0.9`, delete the task branch, and leave remote push to the user.
