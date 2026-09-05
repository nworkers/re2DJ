# 작업 로그: Music Select alpha stage 상태 진단
# Work Log: Music Select Alpha-Stage State Diagnostics

## 1. 작업 개요 (Task Overview)

- **작업 ID**: 191
- **목표**: 같은 `ez2dj4th` Music Select 화면에서 선곡된 곡만 다른 두 출력의 차이를 분석하기 위해 Direct3D stage-0 alpha와 blend 상태를 실제 draw 시점에 확인한다.
- **관련 설계**: [20260905-191-alpha-stage-state-diagnostics.md](../design/20260905-191-alpha-stage-state-diagnostics.md)
- **관련 작업 지시서**: [20260905-191-alpha-stage-state-diagnostics.md](../work-orders/20260905-191-alpha-stage-state-diagnostics.md)

- **Task ID**: 191
- **Objective**: Inspect Direct3D stage-0 alpha and blend state at draw time to analyze the difference between two outputs of the same `ez2dj4th` Music Select screen where only the selected song differs.
- **Related design**: [20260905-191-alpha-stage-state-diagnostics.md](../design/20260905-191-alpha-stage-state-diagnostics.md)
- **Related work order**: [20260905-191-alpha-stage-state-diagnostics.md](../work-orders/20260905-191-alpha-stage-state-diagnostics.md)

## 2. 변경 내용 (Changes)

- `LateDraw` bounded trace에 `alpharef`, `alphafunc`, `alphaop`, `alphaarg1`, `alphaarg2`, `lighting`을 추가했다.
- 추적 전용 변경만 수행했으며, OpenGL 셰이더의 알파·블렌드 의미는 변경하지 않았다.
- 원본 HDD/CHD, 실행 파일, 픽셀 데이터는 저장소 변경에 포함하지 않았다.

- Added `alpharef`, `alphafunc`, `alphaop`, `alphaarg1`, `alphaarg2`, and `lighting` to the bounded `LateDraw` trace.
- Made diagnostic-only changes; no OpenGL shader alpha or blend semantics were changed.
- No original HDD/CHD, executable, or pixel data was added to the repository.

## 3. 검증 (Verification)

### 3.1 빌드 및 단위 테스트 (Build And Unit Tests)

- `cmd /c scripts\build_win32.bat`: 성공, exit code 0.
- `build\windows-x86\bin\Debug\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build\windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 통과.
- 최초 전체 `ctest`는 첫 Windows VFS probe가 장시간 종료되지 않아 해당 검증 프로세스만 중단하고, 변경과 직접 관련된 단위 테스트를 별도로 완료했다.

- `cmd /c scripts\build_win32.bat`: passed, exit code 0.
- `build\windows-x86\bin\Debug\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build\windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 passed.
- The initial full `ctest` run did not terminate promptly at the Windows VFS probe, so that verification process was stopped and the directly relevant unit test was completed separately.

### 3.2 실제 4th Trax draw 관찰 (Real 4th Trax Draw Observation)

사용자가 제공한 `ez2dj4th` CHD를 사용하여 현재 Debug launcher probe를 실행했고, Music Select와 일치하는 draw 패턴 후보를 bounded graphics trace에서 확인했다. 다만 당시 실행 명령에 `--io-config`가 없었으므로 F3 코인과 Enter 입력은 raw I/O 경로에 전달되지 않았다. 따라서 코인 투입 후 사용자가 진입한 Music Select라고 확정할 수 없으며, attract/demo 또는 무입력 경로일 가능성을 남긴다. 원본 자산 자체는 문서에 복사하지 않았다.

- 중앙 artwork draw: `texture=63`, `FVF=0x1c4`, `256x256`, bounds `188.5,47.0`–`451.5,369.0`.
- 중앙 artwork의 alpha stage: `alphaop=4`, `alphaarg1=2`, `alphaarg2=0`.
- 중앙 artwork의 blend: `blend=1`, `srcblend=2`, `dstblend=2`; alpha test off, color key on.
- 양쪽 원판 draw: `texture=62`, 동일한 alpha stage; `srcblend=1`, `dstblend=3`.
- 관측된 Music Select draw에서 `lighting=1`이 기록되었고, 실제 draw FVF는 transformed vertex 형식 `0x1c4`였다.
- 초기 배경 draw 일부만 `alphaop=0`, `alphaarg1=0`, `alphaarg2=0`을 기록했으며 이후 화면 draw는 `MODULATE(TEXTURE, DIFFUSE)` 상태였다.

The current Debug launcher probe was run with the user-supplied `ez2dj4th` CHD, and its bounded graphics trace captured a draw pattern consistent with Music Select. However, the command did not include `--io-config`, so F3 coin and Enter input were not delivered through the raw I/O path. Coin-driven user entry into Music Select is therefore not confirmed; an attract/demo or no-input path remains possible. The original asset itself was not copied into the document.

- Center artwork draw: `texture=63`, `FVF=0x1c4`, `256x256`, bounds `188.5,47.0`–`451.5,369.0`.
- Center artwork alpha stage: `alphaop=4`, `alphaarg1=2`, `alphaarg2=0`.
- Center artwork blend: `blend=1`, `srcblend=2`, `dstblend=2`; alpha test off, color key on.
- Side-disc draw: `texture=62`, same alpha stage; `srcblend=1`, `dstblend=3`.
- The observed Music Select draw recorded `lighting=1`, and the actual draw FVF was transformed-vertex format `0x1c4`.
- Only some initial background draws recorded `alphaop=0`, `alphaarg1=0`, `alphaarg2=0`; later screen draws used `MODULATE(TEXTURE, DIFFUSE)`.

## 4. 결론 (Conclusion)

관측된 draw에서 `alphaop`/`alphaarg`가 셰이더와 다르게 적용되어 중앙 artwork가 밝아진다는 가설은 지지되지 않는다. 관측된 중앙 후보 draw의 alpha 상태가 현재 셰이더의 `texel * v_color`와 일치한다. 다만 코인 투입 후 동일 화면에 진입했다는 전제는 아직 검증하지 못했으므로, 실제 입력을 포함한 재실행 로그가 필요하다. 이번 작업에서는 렌더링 의미를 변경하지 않고, 다음 후보를 컬러키-선형필터 경계와 중앙 artwork의 additive blend 조합으로 좁힌다.

The observed candidate draw does not support the hypothesis that a mismatch between `alphaop`/`alphaarg` and the shader brightens the center artwork. The candidate center draw's alpha state matches the current shader's `texel * v_color`. User-driven entry into the screen remains unverified and requires a rerun with explicit I/O input. This task therefore leaves rendering semantics unchanged and narrows the next candidates to the color-key/linear-filter boundary and the center artwork's additive-blend combination.

## 5. 정정 (Correction)

초기 기록에서 해당 실행을 “Music Select 진입”으로 표현했으나, 실행 명령에 `--io-config`가 빠져 raw I/O 코인 입력이 없었던 사실을 후속 점검에서 확인했다. 위 결과는 draw 패턴과 상태의 관찰 증거로 유지하되, 코인 투입을 포함한 화면 진입 증거로는 사용하지 않는다.

The initial record described this run as having entered Music Select, but a follow-up audit found that `--io-config` was missing, so no raw-I/O coin input was delivered. The draw-pattern and state observations remain valid, but the run must not be used as evidence of coin-driven entry into the screen.

후속으로 사용자가 `20260905-104129-239` 실행에서 직접 코인을 넣고 게임을 진행하여 Music Select까지 진입했다고 확인했다. 이 실행은 `io_port_runtime` 준비 이벤트와 frame `1005`의 중앙 artwork 후보 draw(`texture=387`)를 포함하므로, Music Select alpha/blend 분석용 유효 로그로 기록한다. 다만 키 입력 순간 자체를 별도 이벤트로 남기는 기능은 아직 없다.

As a follow-up, the user confirmed that they inserted a coin, proceeded through the game, and reached Music Select in run `20260905-104129-239`. That run contains the prepared `io_port_runtime` event and the center-artwork candidate draw (`texture=387`) in frame `1005`, so it is recorded as valid evidence for Music Select alpha/blend analysis. Individual keypress moments are not yet emitted as separate events.

## 5. 관련 문서 (Related Documents)

- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)
- [Task 188 작업 로그](20260905-188-directinput7-hle-facade.md)

- [4th Graphics Path Analysis](../analysis/ez2dj4th-graphics-path.md)
- [Task 188 Work Log](20260905-188-directinput7-hle-facade.md)
