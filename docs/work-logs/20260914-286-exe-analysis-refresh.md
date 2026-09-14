# 작업 286 작업 로그 — `ez2dj1.exe` 서술 제거와 실행 파일 분석 갱신 / Task 286 work log — Removing the `ez2dj1.exe` narrative and refreshing the executable analysis

작업 지시: [20260914-286-exe-analysis-refresh.md](../work-orders/20260914-286-exe-analysis-refresh.md)

## 한국어

### 측정

사용자 제공 자산에 대해 저장소 도구로 재측정했다. 원본 파일은 저장소에 넣지 않았고, 기록한 것은 구조·오프셋·개수뿐이다.

| 측정 | 도구 | 결과 |
| --- | --- | --- |
| 1st SE 정식 빌드 원본 `.idata` | IMAGE_IMPORT_DESCRIPTOR 직접 해석 | 7 DLL / 144 함수 |
| 1st SE 정식 빌드 packed `.gidata` | `re2dj_pe_loader` + 직접 해석 | 7 DLL / 161 함수 |
| 3rd CHD 빌드 원본 `.idata` | 직접 해석 | 10 DLL / 159 함수 |
| 4th CHD 빌드 원본 `.idata` | 직접 해석 | 10 DLL / 161 함수 |
| 3rd·4th packed table | `re2dj_pe_loader` | 각 36 항목 |
| `ez2d2m` packed table | `re2dj_pe_loader` | 32 항목 |
| 각 빌드 PE header·섹션 | `re2dj_pe_analyzer` | 문서의 표에 반영 |
| CHD 실행 파일 추출 | `re2dj_chd_probe --dump` | 3rd·4th |

두 해석 경로는 1st SE에서 161개 항목이 이름까지 일치했다. 이 교차 확인으로 어느 한 도구의 해석 오류 가능성을 줄였다.

### 확인된 정정 사항

1. **import 표면 수치.** 기존 문서는 "7 DLL / 144 함수"를 원본의 전체 표면으로 제시했다. 실제로 Windows loader가 bind하는 것은 packed `.gidata`의 161개다. 144개는 원본 `.idata`의 게임 표면이고, 차이 17개는 보호 계층이 더한 것이다.
2. **보호 계층 추가 import 목록.** 기존 9절은 24개 이름을 보호 계층 추가로 적었으나, 그중 `DebugBreak`, `OutputDebugStringA`, `FatalAppExitA`, `UnhandledExceptionFilter`, `RaiseException`, `RtlUnwind`, `IsBadReadPtr`, `IsBadWritePtr`, `HeapValidate` 아홉 개는 원본 `.idata`에 이미 있는 MSVC CRT import였다. 실제 추가분은 정확히 17개다.
3. **`DeviceIoControl`.** 기존 문서는 이 API가 import되지 않는다고 적고 그로부터 `Tdsd.vxd111` 드라이버 비활성 추정을 유도했다. 정식 빌드는 이 API를 import하며, 다만 `.gidata` 전용 항목이므로 **보호 계층의 것**이다. 게임 본체는 장치 IOCTL을 하지 않는다는 더 강한 결론으로 바뀐다.
4. **빌드 시각.** 기존 표의 값 일부는 PE TimeDateStamp가 아니라 파일시스템 날짜였다. 정식 `ez2dj.exe`는 2000-01-01이 아니라 `0x3862df27` = 1999-12-24이고, 2nd `EZ2DJ.exe`는 2004-10-01이 아니라 2004-07-18이다.
5. **3rd는 입력에 따라 다른 빌드다.** 디렉터리 덤프의 `EZ2DJ.EXE`는 `0x3baea943`(2001-09-24), 현재 제품이 실행하는 CHD의 것은 `0x3bca98a3`(2001-10-15)로 3주 차이가 난다. 섹션 크기와 import directory RVA도 다르다. 두 빌드의 RVA는 대부분 같아 주소 하나로는 구분되지 않는다.

### 문서 변경

* `docs/analysis/ez2dj-import-surface.md` — 측정 대상을 정식 빌드로 바꾸고, 1절을 제품별 원본/packed table 비교표와 1st SE·3rd·4th 상세표로 다시 썼다. 9절의 보호 계층 추가 목록을 실측 17개로 정정했다.
* `docs/analysis/ez2dj-exe-structures.md` — 절 하나를 삭제하고 번호를 다시 매겼다. 공통 특성의 타임스탬프 표를 PE TimeDateStamp 기준으로 재작성하고 4th·`ez2d2m`를 추가했다. 3rd CHD 빌드 비교(3.3), 4th PE 구조와 import(4절), `EZ2Dancer.exe` PE 구조(5절)를 새로 넣었다.
* `docs/analysis/ez2dj-hdd-layout.md` — 해당 항목을 삭제하고, 선호 주소 고정 사실을 정식 빌드 기준으로 다시 확인해 넣었다. PE 특성 표에 4th·`ez2d2m` 행을 추가했다. `Tdsd.vxd111` 항목을 정정했다.
* `docs/analysis/ez2dj-demo-volume.md` — 근거를 정식 `ez2dj.exe` `.text` VA `0x004371a6`의 `ff 15 8c a3 eb 01` 직접 확인으로 바꿨다.
* `docs/analysis/windows-original-process-loader.md` — 전체가 해당 빌드 관찰이므로 삭제하고 `docs/analysis/README.md` 색인에서 뺐다.
* `README.md`, `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`, `docs/IMPLEMENTED.md`, `docs/WIN32_HLE_PORTING_PLAN.md`, `docs/guides/hdd-directory-setup.md` — 해당 서술을 빼고 수치를 161/144 구분으로 맞췄다.

### 범위에서 뺀 것

`docs/work-logs/`, `docs/work-orders/`, `docs/design/`는 그대로 뒀다. 날짜가 박힌 작업 증거이며 AGENTS.md의 "work-logs는 시간순 작업 증거로 유지" 규칙을 따른다. 사용자 결정 사항이다.

소스 코드도 그대로 뒀다. `src/target/target_profile.cpp`의 1st SE 덤프 fingerprint 목록과 두 legacy probe 도구의 기본 target id가 아직 그 이름을 쓴다. 문서와 코드가 어긋난 상태이므로 후속 작업 후보다.

### 검증

* 코드 변경 없음. 따라서 빌드 검증은 수행하지 않았다.
* 대상 문서에서 해당 문자열이 남지 않음을 grep으로 확인했다.
* 수정한 12개 문서의 상대 링크가 모두 실제 파일로 해석됨을 확인했다.
* 삭제한 문서를 가리키는 링크가 대상 범위에 남지 않음을 확인했다.
* 수정한 문서의 마크다운 표 구분선과 코드펜스 짝이 맞음을 확인했다.

## English

### Measurement

Re-measured with repository tools against user-supplied assets. No original file entered the repository; only structures, offsets, and counts were recorded.

| Measurement | Tool | Result |
| --- | --- | --- |
| 1st SE canonical original `.idata` | direct IMAGE_IMPORT_DESCRIPTOR parse | 7 DLLs / 144 functions |
| 1st SE canonical packed `.gidata` | `re2dj_pe_loader` + direct parse | 7 DLLs / 161 functions |
| 3rd CHD build original `.idata` | direct parse | 10 DLLs / 159 functions |
| 4th CHD build original `.idata` | direct parse | 10 DLLs / 161 functions |
| 3rd and 4th packed tables | `re2dj_pe_loader` | 36 entries each |
| `ez2d2m` packed table | `re2dj_pe_loader` | 32 entries |
| PE headers and sections per build | `re2dj_pe_analyzer` | reflected in the document tables |
| CHD executable extraction | `re2dj_chd_probe --dump` | 3rd and 4th |

For 1st SE the two parse paths agreed on all 161 entries including names, which reduces the chance that a single tool misparsed the table.

### Confirmed corrections

1. **Import-surface numbers.** The documents presented "7 DLLs / 144 functions" as the complete original surface. What the Windows loader actually binds is the 161-entry packed `.gidata`. The 144 is the game surface in the original `.idata`, and the 17-entry difference is added by the protection layer.
2. **The protection-layer import list.** Section 9 listed 24 names as protection additions, but nine of them — `DebugBreak`, `OutputDebugStringA`, `FatalAppExitA`, `UnhandledExceptionFilter`, `RaiseException`, `RtlUnwind`, `IsBadReadPtr`, `IsBadWritePtr`, `HeapValidate` — are MSVC CRT imports already present in the original `.idata`. The real addition is exactly 17.
3. **`DeviceIoControl`.** The documents stated this API is not imported and inferred from that a disabled `Tdsd.vxd111` driver. The canonical build does import it, but only as a `.gidata` entry, so it belongs to the **protection layer**. This turns into the stronger conclusion that the game body issues no device IOCTL at all.
4. **Build timestamps.** Several table values were filesystem dates rather than PE TimeDateStamps. The canonical `ez2dj.exe` is `0x3862df27` = 1999-12-24, not 2000-01-01, and 2nd `EZ2DJ.exe` is 2004-07-18, not 2004-10-01.
5. **3rd differs by input.** The directory dump `EZ2DJ.EXE` is `0x3baea943` (2001-09-24) while the CHD the product runs carries `0x3bca98a3` (2001-10-15) — three weeks apart, with different section sizes and import-directory RVA. Most RVAs agree, so an address alone does not distinguish them.

### Document changes

* `docs/analysis/ez2dj-import-surface.md` — retargeted to the canonical builds; section 1 rewritten as a per-product original/packed comparison plus detail tables for 1st SE, 3rd and 4th; the section 9 protection-addition list corrected to the measured 17.
* `docs/analysis/ez2dj-exe-structures.md` — one section removed and the rest renumbered; the common-traits timestamp table rebuilt on PE TimeDateStamp with 4th and `ez2d2m` added; new 3.3 (3rd CHD comparison), section 4 (4th PE structure and imports) and section 5 (`EZ2Dancer.exe` PE structure).
* `docs/analysis/ez2dj-hdd-layout.md` — the affected items removed; the preferred-base fact re-established on the canonical build; 4th and `ez2d2m` rows added to the PE characteristics table; the `Tdsd.vxd111` item corrected.
* `docs/analysis/ez2dj-demo-volume.md` — evidence moved to a direct reading of `ff 15 8c a3 eb 01` at `.text` VA `0x004371a6` in the canonical `ez2dj.exe`.
* `docs/analysis/windows-original-process-loader.md` — removed, since the whole document recorded observations on that build, and dropped from the `docs/analysis/README.md` index.
* `README.md`, `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`, `docs/IMPLEMENTED.md`, `docs/WIN32_HLE_PORTING_PLAN.md`, `docs/guides/hdd-directory-setup.md` — the narrative removed and the numbers aligned to the 161/144 distinction.

### Excluded from scope

`docs/work-logs/`, `docs/work-orders/` and `docs/design/` were left untouched as dated task evidence, under the AGENTS.md rule that work logs remain chronological evidence. This was a user decision.

Source code was also left untouched. The 1st SE dump fingerprint list in `src/target/target_profile.cpp` and the default target id of two legacy probe tools still use that name, so documents and code now disagree there; that is a follow-up candidate.

### Verification

* No code changed, so no build verification was run.
* Confirmed by grep that the string no longer appears in the target documents.
* Confirmed that every relative link in the twelve edited documents resolves to a real file.
* Confirmed that no link in the target scope points at the removed document.
* Confirmed balanced markdown table separators and code fences in the edited documents.
