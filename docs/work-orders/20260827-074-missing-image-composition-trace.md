# 누락 이미지 합성 추적 및 복구 작업 지시

관련 설계: [누락 이미지 합성 추적 및 복구](../design/20260827-074-missing-image-composition-trace.md)

## 상태

**진행 중.** 사용자 재검증으로 마스킹·컬러키 개선과 잔여 누락 그림이 확인됐다.

## 작업

1. DirectDraw surface에 runtime 진단 ID를 부여한다.
2. `CreateSurface`, `GetDC`/`ReleaseDC`, `Blt`, `BltFast`, `Flip`의 인자·결과를 bounded marker로 기록한다.
3. x86/x64 warnings-as-errors build와 CTest로 진단 변경을 검증한다.
4. debugger mode 원본 실행으로 누락 화면 구간의 실제 호출을 수집한다.
5. 확인된 누락 경계를 공용 core와 Windows backend 책임에 맞춰 구현하고 단위 테스트한다.
6. 사용자 detached 화면에서 누락 그림과 기존 마스킹·컬러키를 재검증한다.
7. architecture, TODO, IMPLEMENTED, analysis와 작업 로그를 결과에 맞춰 갱신하고 커밋한다.

---

# Missing-image Composition Trace and Recovery Work Order

Related design: [Missing-image Composition Trace and Recovery](../design/20260827-074-missing-image-composition-trace.md)

**In progress.** User revalidation confirms corrected masking/color keying and remaining missing imagery. Add bounded DirectDraw composition markers, verify them with warnings-as-errors builds and CTest, collect the actual call sequence in debugger mode, implement only the confirmed missing boundary with neutral-core tests, revalidate detached output, update the architecture/analysis/status documents and work log, and commit the completed task.

## 추가 진단 / Additional diagnosis

사용자 피드백에 따라 BMP `CreateFileA`의 요청 경로, 매핑 경로, 성공 여부와 Win32 오류를 별도 bounded VFS 로그로 수집해 surface 생성 이전의 파일 로딩 실패를 확인한다.

Following user feedback, collect BMP `CreateFileA` request paths, mapped paths, success status, and Win32 errors in a separate bounded VFS log to identify file-loading failures before surface creation.

`logo.str` 내부 `AMUSEWORLD_OBJ256` 객체가 표시되는 초기 구간에 대해 texture 내용, draw 좌표와 render-state 변경을 수집하고 검은 전체 화면 quad의 합성 의미를 확인한다.

For the early segment that displays the `AMUSEWORLD_OBJ256` object embedded in `logo.str`, collect texture contents, draw bounds, and render-state changes, then determine the composition semantics of the full-screen black quad.

회사 로고 BMP가 `LoadImageA`로 `CreateFileA` VFS를 우회하는 경계를 구현한다. 확인된 상대경로 `IMAGE_BITMAP | LR_LOADFROMFILE`만 HDD/overlay 읽기 경로로 매핑하고, Windows VFS probe와 x86/x64 build·CTest로 검증한다.

Implement the boundary where company-logo BMP loading through `LoadImageA` bypasses the `CreateFileA` VFS. Map only the confirmed relative-path `IMAGE_BITMAP | LR_LOADFROMFILE` case to the HDD/overlay read path, then verify it with the Windows VFS probe, x86/x64 builds, and CTest.

## `.str` 경계 진단 확장 / `.str` boundary diagnostic extension

`LoadImageA` wrapper는 완료됐고 정적 분석·런타임 로그 대조로 동작이 확인됐다. 그러나 로고 자산 요청 자체가 0건이라 누락 경계가 `.str` 로딩 앞단으로 옮겨졌다. 확인·기각된 내용은 [자산 로딩 경로 분석](../analysis/ez2dj-asset-loading-path.md)과 설계 문서에 반영했다.

*The `LoadImageA` wrapper is complete and verified by static analysis cross-checked against runtime logs, but zero logo-asset requests move the missing boundary upstream to `.str` loading. Confirmed and rejected findings are recorded in the [asset loading path analysis](../analysis/ez2dj-asset-loading-path.md) and the design document.*

1. VFS 진단 필터를 `.bmp`와 `.str`로 넓힌다.
2. marker에 호출 API(`CreateFileA` / `LoadImageA`)를 표기해 존재 확인과 실제 로딩을 직접 구분한다.
3. 진단 상한을 확장자별로 분리해 BMP 스윕이 `.str` 증거를 가리지 못하게 한다.
4. `Re2djVfsCreateFileA` 매핑 실패 분기의 보고·`SetLastError` 순서를 정상 경로와 맞춰 게스트가 볼 오류 코드 오염을 없앤다.
5. 런처의 `LoadImageA` IAT 패치 준비 상태를 분리하고 실패를 진단 이벤트로 남겨, 뒤따르는 `DeviceIoControl` 목 패치가 조용히 생략되지 않게 한다.
6. Windows VFS probe에 `.str` 경계 회귀를 추가하고 x86/x64 warnings-as-errors build와 CTest로 검증한다.
7. 사용자 detached 재실행으로 `System\CompanyLogo\logo.str` 요청 유무와 결과를 수집한다. **(완료 — `20260827-015256`에서 open 성공 확인)**

*Widen the VFS diagnostic filter to `.bmp` and `.str`; tag each marker with the calling API so existence probes and real loads are directly distinguishable; give each extension its own bound so the BMP sweep cannot hide `.str` evidence; align the report and `SetLastError` order in the `Re2djVfsCreateFileA` mapping-failure branch to stop the diagnostic from corrupting the guest-visible error; separate the launcher's `LoadImageA` IAT patch readiness and record failures as a diagnostic event so the following `DeviceIoControl` mock patch is never silently skipped; add a `.str` boundary regression to the Windows VFS probe and verify with x86/x64 warnings-as-errors builds and CTest; then collect whether `System\CompanyLogo\logo.str` is requested, and with what result, from a detached user re-run.*

## `.str` 읽기 경계 수정 / `.str` read boundary fix

`20260827-015256` 로그에서 `logo.str`·`title.str` open이 모두 성공했고, 원본 `.str` 로더가 `FILE_FLAG_NO_BUFFERING`으로 열어 sector 배수가 아닌 크기를 통째로 읽는 것이 확인됐다. 호스트 재현에서 `ReadFile`이 `ERROR_INVALID_PARAMETER`로 0바이트를 전송한다.

*The `20260827-015256` log shows both `logo.str` and `title.str` opening successfully, and the original `.str` loader opens with `FILE_FLAG_NO_BUFFERING` before reading a whole non-sector-multiple file; host reproduction fails that `ReadFile` with `ERROR_INVALID_PARAMETER` and zero bytes.*

1. VFS `CreateFileA` 경계에서 `FILE_FLAG_NO_BUFFERING`만 제거하고 나머지 flag는 그대로 전달한다.
2. Windows VFS probe에 sector 배수가 아닌 파일을 `FILE_FLAG_NO_BUFFERING`으로 열어 전체를 읽는 회귀를 추가한다.
3. x86/x64 warnings-as-errors build와 CTest로 검증한다.
4. 사용자 detached 재실행으로 로고와 Title 장면 그래픽이 표시되는지, 기존 마스킹·컬러키가 회귀하지 않는지 확인한다.

*Strip only `FILE_FLAG_NO_BUFFERING` at the VFS `CreateFileA` boundary while forwarding every other flag; add a Windows VFS probe regression that opens a non-sector-multiple file with `FILE_FLAG_NO_BUFFERING` and reads it whole; verify with x86/x64 warnings-as-errors builds and CTest; then confirm in a detached user re-run that the logo and Title scene graphics appear without regressing the corrected mask and color key.*
