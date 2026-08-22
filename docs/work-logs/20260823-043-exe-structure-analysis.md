# 실행 파일 구조 문서 정리 작업 로그

관련 작업 지시: [실행 파일 구조 문서 정리 작업 지시](../work-orders/20260823-043-exe-structure-analysis.md)

## 결과

`docs/analysis/ez2dj-exe-structures.md`를 새로 만들었다. 지금까지 [HDD 레이아웃 분석](../analysis/ez2dj-hdd-layout.md), [import 표면 분석](../analysis/ez2dj-import-surface.md), 작업 로그 036~042에 흩어져 있던 실행 파일별 구조 지식을 실행 파일 단위로 재편했다.

### 문서 구성

1. **공통 특성**: 다섯 실행 파일의 공통 PE 특성과 `dll flags 0x0000`(ASLR·DEP 미선호), `ez2dj1.exe`와 `ez2dj.exe`의 동일 타임스탬프 `0x3862df27`.
2. **`ez2dj1.exe`**: 섹션 테이블, 빈 relocation directory, `.data` 27 MB 0 채움 추정.
3. **`ez2dj.exe`**(가장 상세): 섹션 테이블, `.gidata` import 재배치, `.gtide` 해부(안티디스어셈블 점프, XOR 루프, 자기 수정 확인 근거, TLS 부재), `.gdata` VA 인벤토리(WSOCK32.DLL·WSAGetLastError·MSVBVM50.DLL 쌍, `\\.\TDSD.VXD`, `\\.\LPTDI0`, 해시 blob), 런타임 흐름 Mermaid 도식, fault 서명.
4. **`EZ2DJ.EXE`(3rd)**: 섹션 테이블, RWX 플래그의 `.protect`, import·reloc directory가 모두 `.protect` 안에 있음을 새로 확인해 기록.
5. **보조 도구**: `Test.exe`(재배치 가능 확인), `PlzPowerOff.exe`.
6. **새 실행 파일 추가 절차**: 측정 명령, 섹션 골격, 색인 갱신 규칙.

### 이번 정리에서 새로 확인한 사실

* `ez2dj1.exe`와 `ez2dj.exe`의 PE TimeDateStamp가 `0x3862df27`로 동일하다(`re2dj_pe_analyzer`). 보호 처리가 타임스탬프를 보존했고 두 파일이 같은 원본 빌드라는 기존 결론을 강화한다.
* 3rd `EZ2DJ.EXE`의 import directory(RVA `0x0067af90`)와 base relocation directory(RVA `0x00643000`)가 모두 `.protect` 가상 범위 안에 있다. `.protect` 플래그 `0xe0000020`은 write 비트를 포함한다 — 1st SE `.gtide`(write 없음)와 다르다.
* `Test.exe`는 resource와 비어 있지 않은 relocation directory를 가져 재배치 가능하다. `PlzPowerOff.exe`는 characteristics `0x010f`로 나머지(`0x010e`)와 다르다.
* 보호 이미지에는 TLS data directory가 없다. 진입 전 TLS callback 수정 가능성을 배제하는 근거다.

### 갱신한 연결 문서

* `docs/analysis/README.md`: 구조 문서 행 추가, import 표면 상태 갱신.
* `docs/EXE_DESIGN.ko.md` / `.en.md`: 1절 링크에 구조 문서 추가, 2.4 동글 행을 미확정 → 부분 확인으로 갱신(양쪽 언어 함께).

## 검증

코드 변경이 없으므로 빌드 검증은 생략했다. 대신 문서 수치의 출처를 전부 도구 출력으로 대조했다: 다섯 실행 파일의 헤더·섹션 표는 이번 작업에서 `re2dj_pe_analyzer`를 다시 실행해 확보했고, `.gdata` VA는 파일 오프셋 덤프에 `+0x01E51000` 변환을 적용해 기존 런타임 관찰 값(`0x01ed79b8`, `0x01ed79c4`, `0x01ed7a4c`)과 교차 검증했다.

---

# Executable Structure Documentation Work Log

Related work order: [Executable Structure Documentation Work Order](../work-orders/20260823-043-exe-structure-analysis.md)

## Result

Created `docs/analysis/ez2dj-exe-structures.md`, reorganizing per-executable structure knowledge previously scattered across the HDD layout analysis, the import surface analysis, and work logs 036–042.

### Document composition

Common traits; `ez2dj1.exe`; a detailed `ez2dj.exe` section (sections, `.gidata` import relocation, `.gtide` anatomy with self-modification evidence and absent TLS, a `.gdata` VA inventory, a runtime-flow Mermaid diagram, and the fault signature); 3rd `EZ2DJ.EXE` (RWX `.protect` owning both directories); auxiliary tools; and the procedure for adding a new executable.

### Newly confirmed during this consolidation

* Identical PE timestamp `0x3862df27` for `ez2dj1.exe` and `ez2dj.exe`.
* Third's import (RVA 0x0067af90) and relocation (RVA 0x00643000) directories both live inside the `.protect` range, whose flags `0xe0000020` include write — unlike 1st SE's `.gtide`.
* `Test.exe` is relocatable (non-empty relocation directory plus resources); `PlzPowerOff.exe` carries characteristics `0x010f`.
* The protected image has no TLS data directory, excluding pre-entry TLS-callback modification.

### Updated companion documents

Analysis README index, and EXE_DESIGN ko/en links plus the dongle row moved from unresolved to partially confirmed in both languages.

## Verification

No code change, so build verification is skipped. Every number was cross-checked against tool output: header/section tables were re-captured with `re2dj_pe_analyzer` this task, and `.gdata` VAs were converted from file offsets (`+0x01E51000`) and cross-checked against prior runtime observations.
