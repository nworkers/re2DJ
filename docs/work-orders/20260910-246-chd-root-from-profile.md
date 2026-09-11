# 작업 지시서: CHD 내부 디렉터리를 프로파일에서 받기

## 한국어

### 배경

runtime의 `ChdRelativePath`가 이미지 내부 제품 디렉터리를 `"EZ2DJ/"`로 하드코딩합니다. `ez2d2m`은 `ez2dancer` 아래에 있으므로 CHD fallback이 항상 빗나가고, 게스트가 자기 `EZ2Dancer.ini`를 열지 못합니다. 이 저장소 첫 비-EZ2DJ 제품이 드러낸 가정입니다. 경위는 [종료 원인 정정 절](../analysis/ez2d2m-chd-filesystem.md)에 있습니다.

별도 설계 문서를 만들지 않습니다. 하드코딩을 프로파일이 이미 가진 값으로 바꾸는 국소 수정이며 새 구조를 도입하지 않습니다.

### 작업 항목

1. runtime에 CHD 내부 루트 전역을 추가하고 `ChdRelativePath`가 그것을 쓰게 합니다. 기본값은 기존 동작과 같은 `EZ2DJ`로 둡니다.
2. launcher가 프로파일의 `executable_relative_path`에서 디렉터리 부분을 뽑아 전달하게 합니다.
3. `ez2d2m`이 CHD에서 `EZ2Dancer.ini`를 여는지 확인합니다.
4. byte 제품에 회귀가 없는지 확인합니다.
5. Windows x86 build와 단위 시험을 검증합니다.

### 제외 범위

- `function=0x0011` 응답
- guest-memory writer

### 완료 조건

- `ez2d2m`의 INI open이 `stage=chd success=1`입니다.
- `ez2dj3rd`의 실행 결과가 변하지 않습니다.

## English

### Background

The runtime's `ChdRelativePath` hard-codes the product's directory inside the image as `"EZ2DJ/"`. `ez2d2m` lives under `ez2dancer`, so every CHD fallback for it misses and the guest cannot open its own `EZ2Dancer.ini` — an assumption exposed by this repository's first non-EZ2DJ product. The circumstances are in [the corrected exit-cause section](../analysis/ez2d2m-chd-filesystem.md).

No separate design document: this replaces a hard-coded string with a value the profile already carries and introduces no new structure.

### Work items

1. Add a runtime global for the image-internal root and have `ChdRelativePath` use it, defaulting to `EZ2DJ` so existing behaviour is unchanged.
2. Have the launcher derive it from the profile's `executable_relative_path` directory component.
3. Confirm `ez2d2m` opens `EZ2Dancer.ini` from the CHD.
4. Confirm no regression for a byte product.
5. Verify the Windows x86 build and unit tests.

### Out of scope

- The `function=0x0011` response
- A guest-memory writer

### Completion criteria

- `ez2d2m`'s INI open reports `stage=chd success=1`.
- `ez2dj3rd`'s run is unchanged.
