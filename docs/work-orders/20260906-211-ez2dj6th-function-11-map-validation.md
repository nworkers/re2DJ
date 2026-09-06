# 작업 지시서: EZ2DJ 6th Function 0x11 map 검증

## 한국어

### 목표

실제 6th transform 입력을 사용해 기존 194개 seed 후보의 map을 재생성하고, Function `0x0011`이 기존 reSoftlock의 Function `0x000e` 변환과 호환되는지 검증합니다.

### 절차

1. 6th EXE의 정적 challenge 수와 runtime transform 입력 수를 비교합니다.
2. runtime 입력을 challenge 파일로 사용해 reSoftlock `map-batch`를 실행합니다.
3. candidate map을 6th CHD bootstrap/child 경로에 주입합니다.
4. `mapped=7:unmapped=0`, child 종료 코드, crash context, 후속 descriptor 및 `EZ2DJ.ini` 접근을 수집합니다.
5. map 미주입 baseline과 비교해 response map의 영향과 인증 성공을 분리합니다.

### 완료 조건

- static/runtime challenge 경계가 확인됩니다.
- 194개 map이 7개 입력에 대해 완전히 매칭됩니다.
- Function `0x0011`의 동일 알고리즘 적용 여부를 확정하거나, 필요한 외부 증거를 명시합니다.

## English

### Objective

Regenerate maps for the 194 existing seed candidates from the real 6th transform inputs and test whether Function `0x0011` is compatible with reSoftlock's Function `0x000e` transform.

### Procedure

1. Compare the static challenge count from the 6th executable with the runtime transform input count.
2. Run reSoftlock `map-batch` using the runtime inputs as the challenge file.
3. Inject each candidate map through the 6th CHD bootstrap/child path.
4. Collect `mapped=7:unmapped=0`, child exit code, crash context, later descriptors, and `EZ2DJ.ini` access.
5. Compare with a no-map baseline to separate map effects from authentication success.

### Done when

- The static/runtime challenge boundary is established.
- All 194 maps fully match the seven inputs.
- Compatibility of the Function `0x0011` algorithm is either confirmed or the required external evidence is identified.
