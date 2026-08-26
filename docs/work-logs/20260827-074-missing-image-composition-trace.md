# 누락 이미지 합성 추적 작업 로그

관련 설계: [누락 이미지 합성 추적 및 복구](../design/20260827-074-missing-image-composition-trace.md)  
관련 작업 지시: [누락 이미지 합성 추적 및 복구](../work-orders/20260827-074-missing-image-composition-trace.md)  
관련 분석: [자산 로딩 경로](../analysis/ez2dj-asset-loading-path.md)

## `LoadImageA` 경계 검토

원본 `ez2dj.exe`(1st SE, 보호됨) 정적 분석과 detached 실행 로그를 대조했다.

- `USER32!LoadImageA` 호출 지점은 `.text`에 두 곳(`0x0041ffc0`, `0x00422c0c`)뿐이고, 인자는 두 곳 모두 `LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION)`다. 기존 wrapper의 게이트 조건과 일치한다.
- `%s.bmp` 포맷 문자열(VA `0x00454e6c`)의 참조처가 이 두 지점뿐이므로, 이름으로 여는 모든 BMP는 wrapper를 지난다.
- 이름 해석은 `sub_00423F70`이 담당한다. VA `0x01c4c0a0`의 `MAX_PATH` 항목 테이블(용량 16, 개수 변수 VA `0x01c4d0e0`)을 순회하며 `<dir>\<name>`을 만들고, `sub_00423F30`이 `CreateFileA`+`CloseHandle`로 존재만 확인한다. 못 찾으면 `LoadImageA`를 호출하지 않는다.
- 보호 빌드는 IAT가 두 벌이다. PE import directory는 `.gidata`(RVA `0x01ad8000`)를 가리키지만 원본 `.text`는 레거시 `.idata`(RVA `0x01aba000`)만 참조한다. 그럼에도 `.gidata` 슬롯 패치가 원본 호출에 반영되는 것은 이미 `DirectDrawCreate`·`CreateFileA`로 확인된 사실이며, `LoadImageA`도 같았다.
- 런타임 대조: wrapper 도입 전 실행(`20260827-005523`, `-010020`, `-010643`)은 성공 BMP 1건당 로그 1줄, 도입 후 실행(`20260827-011204`)은 2줄이고 실패 항목은 양쪽 모두 1줄이다. 위 제어 흐름과 정확히 일치하므로 IAT 패치가 실제로 원본 호출을 가로챈다.

## 원인 재귀속

`LoadImageA` 후킹이 살아 있는 실행에서도 `System\CompanyLogo\` 요청은 **0건**이다. `title.str`이 참조할 `System\Title\` 자산도 0건이고, 실제로 로드되는 것은 `.str`과 무관한 공용 패널 오버레이와 ClubMix 플레이 자산뿐이다. 같은 실행의 DirectDraw 로그에도 256×256이나 500×500 surface가 없다.

따라서 설계의 최초 가설 "회사 로고 로더가 `CreateFileA` 경계를 우회한다"를 기각했다. 로고 자산은 우회되는 것이 아니라 요청되지 않는다. `logo.str`은 아카이브가 아니라 `AMUSEWORLD_OBJ256`을 포함한 오브젝트 이름과 키프레임을 담은 스크립트이고, `.str`이 없는 유일한 화면 그룹 `System\WarningMsg`만 정상 표시된다. 누락 경계는 이미지 로더 앞단의 `.str` 로딩·해석으로 이동했다. 확인·추정·미확정 구분은 분석 문서에 반영했다.

## 구현

- 자산 진단 필터를 `.bmp`에서 `.bmp` + `.str`로 넓히고 확장자 판정을 공용 헬퍼로 분리했다.
- marker를 `re2dj:vfs:asset-open`으로 바꾸고 호출 API(`api=CreateFileA` / `api=LoadImageA`)를 명시했다. 이전에는 줄 중복 패턴으로 간접 추론해야 했다.
- 진단 상한을 확장자별로 분리했다. 어트랙트 루프의 BMP 스윕(고유 경로 776개)이 단일 공용 상한을 모두 소진해 뒤늦은 `.str` 요청을 가리는 문제를 없앤다.
- `Re2djVfsCreateFileA`의 매핑 실패 분기에서 보고와 `SetLastError` 순서를 정상 경로와 맞췄다. 이전에는 진단 자체의 `CreateFileA`가 게스트가 읽을 last error를 덮어썼다.
- 런처의 `LoadImageA` IAT 패치 준비 상태를 `image_loader_prepared`로 분리하고 `vfs_image_loader` 진단 이벤트를 남겼다. 이전에는 이 단계 실패가 뒤따르는 `DeviceIoControl` 목 패치를 조용히 건너뛰었다.

## 검증

- Windows x86 warnings-as-errors 전체 빌드 통과.
- Windows x64 warnings-as-errors 전체 빌드 통과.
- Windows x86 CTest 2/2 통과, Windows x64 CTest 1/1 통과.
- VFS probe에 `.str` 상대경로 열기·읽기, 매핑 불가 경로의 `ERROR_INVALID_NAME` 유지, 트레이스의 `api=CreateFileA`·`api=LoadImageA`·script marker 회귀를 추가했다.
- 오류 코드 회귀가 실제로 동작하는지 확인하려고 보고/`SetLastError` 순서를 일시적으로 되돌려 빌드했고, probe가 `error 183`(`ERROR_ALREADY_EXISTS`)으로 실패했다. 순서를 복구한 뒤 다시 통과한다.

## `.str` 원인 확정과 수정

사용자 detached 재실행 `20260827-015256`의 `.vfs.log`가 결론을 냈다.

- `System\CompanyLogo\logo.str`과 `System\Title\title.str`은 존재 확인과 실제 열기 모두 `success=1`이다. 파일 부재도 경로 매핑 실패도 아니다.
- `.str` 로더는 `CreateFileA(..., OPEN_EXISTING, 0x20000080)` → `GetFileSize` → 파일 크기 전체 `ReadFile`(버퍼 `base + 0x1300c`) → `CloseHandle` 순서다. flags `0x20000080`은 `FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING`이고, BMP 존재 확인이 쓰는 `0x00000080`과 다르다.
- 호스트에서 실제 `logo.str`(52,892 B)로 같은 인자를 재현했다. `0x20000080`은 open 성공·`ReadFile` 실패(`ERROR_INVALID_PARAMETER`, 0바이트), `0x00000080`은 52,892바이트 전부 읽기 성공이다.

빈 버퍼를 파싱하면 오브젝트가 만들어지지 않으므로 그 이름이 `%s.bmp`로 이어지지 않고 `LoadImageA`도 호출되지 않는다. 로고와 Title 장면 그래픽이 함께 비는 관찰이 전부 설명된다.

수정은 VFS `CreateFileA` 경계에서 `FILE_FLAG_NO_BUFFERING`만 제거하는 것이다. 나머지 flag는 그대로 전달하고, 캐싱 정책만 달라지며 게스트가 받는 바이트는 동일하다. 원본 instruction과 자산은 변경하지 않았다.

## 수정 검증

- Windows x86/x64 warnings-as-errors 전체 빌드 통과, x86 CTest 2/2, x64 CTest 1/1 통과.
- VFS probe에 sector 배수가 아닌 스크립트를 `FILE_FLAG_NO_BUFFERING`으로 열어 전체를 읽는 회귀를 추가했다.
- 이 회귀가 실제로 동작하는지 확인하려고 마스킹을 일시 제거해 빌드했고 probe가 실패했다. 마스킹을 복구한 뒤 다시 통과한다.

## 남은 확인

로고와 Title 장면 그래픽이 실제 화면에 표시되는지, 기존 마스킹·컬러키가 회귀하지 않는지는 사용자 detached 재실행으로 확인한다.

---

# Missing-image Composition Trace Work Log

## `LoadImageA` boundary review

Static analysis of the protected 1st SE `ez2dj.exe`, cross-checked against detached run logs, establishes the following. `USER32!LoadImageA` is called from only two `.text` sites, `0x0041ffc0` and `0x00422c0c`, both with `LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION)`, matching the existing wrapper's gate. The `%s.bmp` format string at VA `0x00454e6c` is referenced only by those two sites, so every name-resolved BMP passes through the wrapper. Name resolution belongs to `sub_00423F70`, which walks a `MAX_PATH`-entry table at VA `0x01c4c0a0` (capacity 16, count variable at VA `0x01c4d0e0`), builds `<dir>\<name>`, and probes existence with `sub_00423F30` using `CreateFileA` plus `CloseHandle`; when nothing is found `LoadImageA` is never called.

The protected build carries two import tables: the PE import directory points at `.gidata` (RVA `0x01ad8000`) while the original `.text` references only the legacy `.idata` (RVA `0x01aba000`). Patching the `.gidata` slot nevertheless reaches the original call, already established for `DirectDrawCreate` and `CreateFileA`, and `LoadImageA` behaved the same. The runtime cross-check is decisive: runs before the wrapper (`20260827-005523`, `-010020`, `-010643`) log one line per successful BMP, the run after it (`20260827-011204`) logs two, and failures stay at one line in both — exactly the control flow above, so the IAT patch really intercepts the original call.

## Cause reattribution

Even with the hook active, `System\CompanyLogo\` is requested zero times, as are the `System\Title\` assets `title.str` would reference; only `.str`-independent panel overlays and ClubMix play assets load, and the DirectDraw log for that run contains no 256×256 or 500×500 surface. The design's original hypothesis that the company-logo loader bypasses the `CreateFileA` boundary is therefore rejected — the assets are never requested. `logo.str` is not an archive but a script naming objects including `AMUSEWORLD_OBJ256`, and `System\WarningMsg`, the only screen group with no `.str`, is the group that renders. The missing boundary moves upstream to `.str` loading and interpretation, recorded in the analysis document with confirmed, inferred, and unresolved marks.

## Implementation

The asset diagnostic filter widens from `.bmp` to `.bmp` plus `.str` with extension matching factored into a shared helper; the marker becomes `re2dj:vfs:asset-open` and names the calling API, removing the need to infer loads from duplicated lines; per-extension budgets stop the attract loop's 776-path bitmap sweep from exhausting a single shared bound and hiding a later `.str` request; the mapping-failure branch of `Re2djVfsCreateFileA` now reports before committing `SetLastError`, so the diagnostic's own `CreateFileA` no longer overwrites the guest-visible error; and the launcher tracks the `LoadImageA` IAT patch as `image_loader_prepared` with a `vfs_image_loader` diagnostic event, so a failure there no longer silently skips the following `DeviceIoControl` mock patch.

## Verification

Windows x86 and x64 warnings-as-errors builds pass, x86 CTest passes 2/2, and x64 CTest passes 1/1. The VFS probe gains regressions for relative `.str` open and read, `ERROR_INVALID_NAME` preservation on an unmappable path, and the presence of `api=CreateFileA`, `api=LoadImageA`, and script markers in the trace. To confirm the error-code regression actually bites, the report/`SetLastError` order was temporarily reverted and rebuilt; the probe failed with `error 183` (`ERROR_ALREADY_EXISTS`), and it passes again with the order restored.

## Confirmed `.str` cause and fix

The `.vfs.log` from detached re-run `20260827-015256` settles it. `System\CompanyLogo\logo.str` and `System\Title\title.str` both report `success=1` at the existence probe and the real open, so neither a missing file nor a path-mapping failure is involved. The `.str` loader runs `CreateFileA(..., OPEN_EXISTING, 0x20000080)`, `GetFileSize`, a whole-file `ReadFile` into buffer `base + 0x1300c`, then `CloseHandle`; those flags are `FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING`, unlike the `0x00000080` the BMP existence probe uses. Reproducing the same arguments on the host against the real 52,892-byte `logo.str` shows `0x20000080` opening successfully but failing `ReadFile` with `ERROR_INVALID_PARAMETER` and zero bytes, while `0x00000080` reads all 52,892 bytes.

Parsing an empty buffer produces no objects, so none of their names reach `%s.bmp` and `LoadImageA` is never called, which fully explains the logo and Title scene graphics being absent together. The fix strips `FILE_FLAG_NO_BUFFERING` alone at the VFS `CreateFileA` boundary, forwarding every other flag; only the caching policy changes while the bytes the guest receives stay identical, and no original instruction or asset was modified.

## Fix verification

Windows x86 and x64 warnings-as-errors builds pass, x86 CTest passes 2/2, and x64 CTest passes 1/1. The VFS probe gains a regression that opens a non-sector-multiple script with `FILE_FLAG_NO_BUFFERING` and reads it whole. To confirm that regression bites, the mask was temporarily removed and rebuilt; the probe failed, and it passes again with the mask restored.

## Remaining

Whether the logo and Title scene graphics actually appear on screen, without regressing the corrected mask and color key, must come from a detached user re-run.
