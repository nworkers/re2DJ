# 작업 지시서: ez2dj1stse DirectDraw HLE 활성화

## 한국어

### 관련 문서

- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)
- 선행 작업: [VFS 게스트 루트 접두사와 CHD 열거 현재 디렉터리](20260908-225-vfs-guest-root-and-chd-enumeration.md)

### 배경

`ez2dj1stse`의 `hle_d3d3`는 [작업 223](20260908-223-ez2dj1stse-chd-profile-correction.md)에서 꺼졌습니다. CHD `.protect` 빌드의 PE header가 가리키는 packed import directory에 `DirectDrawCreate`도 `DirectDrawCreateEx`도 없어 준비가 실패하고 게스트 실행 자체가 중단됐기 때문입니다.

원본 `.idata`(RVA `0x01aba000`)는 파일에 그대로 남아 있고 `DDRAW.dll!DirectDrawCreate`를 import합니다. header가 더 이상 그 표를 가리키지 않을 뿐입니다.

### 작업 목표

IAT 조회가 header의 import directory에서 찾지 못했을 때 원본 `.idata` 표까지 보도록 확장하고, `ez2dj1stse`의 `hle_d3d3`를 켭니다.

### 작업 항목

1. `FindIatSlotsByName`이 header directory를 먼저 보고, 못 찾으면 `.idata` section의 표를 추가로 조회하게 합니다. header directory만 권위 있는 것으로 두고, 보조 표의 파싱 실패는 오류가 아니라 불일치로 처리합니다.
2. `ez2dj1stse`의 `hle_d3d3`를 `true`로 바꿉니다.
3. target profile unit test와 product loader probe 인자 계약을 갱신합니다.
4. Windows x86 build와 시험을 검증합니다.
5. 실제 실행으로 DirectDraw 경계 도달과 렌더 루프 진입을 확인합니다.
6. 3rd·4th 실행으로 회귀가 없는지 확인합니다.
7. analysis, ARCHITECTURE, work log를 갱신합니다.

### 제외 범위

- `DirectDrawCreateEx` 경로 변경
- Direct3D·blending 동작 변경
- packed image에서 실패하는 다른 경계(`hle_windows_directory`, `demo_volume`) 활성화. 두 import는 어느 표에도 없음
- Hardlock·VFS 정책 변경

### 완료 조건

- `graphics_trace`가 `has_create=true`를 보고합니다.
- 게스트가 DirectDraw 창을 만들고 surface를 생성해 프레임을 진행합니다.
- unit test와 product loader probe가 통과합니다.
- 3rd·4th의 `has_create`/`has_create_ex`/`create_ex_patched`가 변하지 않습니다.

## English

### Related documents

- Analysis: [ez2dj1stse CHD filesystem analysis](../analysis/ez2dj1stse-chd-filesystem.md)
- Preceding task: [VFS guest root prefix and CHD enumeration working directory](20260908-225-vfs-guest-root-and-chd-enumeration.md)

### Background

`hle_d3d3` was disabled for `ez2dj1stse` in [task 223](20260908-223-ez2dj1stse-chd-profile-correction.md) because the packed import directory the CHD `.protect` build's PE header points at carries neither `DirectDrawCreate` nor `DirectDrawCreateEx`, so preparation failed and aborted the run before the guest executed.

The original `.idata` at RVA `0x01aba000` survives in the file and imports `DDRAW.dll!DirectDrawCreate`; the header simply no longer points at that table.

### Objective

Extend the IAT lookup to consult the original `.idata` table when the header's import directory yields nothing, then enable `hle_d3d3` for `ez2dj1stse`.

### Work items

1. Make `FindIatSlotsByName` search the header directory first and the `.idata` section's table only when that finds nothing, treating the header directory as the sole authoritative table so a malformed secondary table is a non-match rather than an error.
2. Set `hle_d3d3` to `true` for `ez2dj1stse`.
3. Update the target-profile unit test and the product-loader probe's argument contract.
4. Verify the Windows x86 build and tests.
5. Confirm by real run that the DirectDraw boundary is reached and a render loop starts.
6. Confirm no regression with 3rd and 4th runs.
7. Update the analysis document, `ARCHITECTURE.md`, and the work log.

### Out of scope

- Changing the `DirectDrawCreateEx` path
- Changing Direct3D or blending behavior
- Enabling other boundaries that fail on this packed image, such as `hle_windows_directory` and `demo_volume`, whose imports are in neither table
- Changing Hardlock or VFS policy

### Completion criteria

- `graphics_trace` reports `has_create=true`.
- The guest creates a DirectDraw window and surfaces and advances frames.
- The unit tests and the product-loader probe pass.
- 3rd and 4th keep their existing `has_create`, `has_create_ex`, and `create_ex_patched` values.
