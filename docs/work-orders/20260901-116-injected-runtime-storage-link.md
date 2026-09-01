# injected runtime CHD 저장소 링크 수정 작업 지시

## 한국어

### 목표

`re2dj_windows_injected_runtime.dll` Debug x86 링크 실패를 수정해 `re2dj.exe ez2dj4th --run`이 번들 injected runtime을 찾을 수 있는 빌드 산출물을 만들도록 합니다.

### 작업 범위

1. `src/storage/guest_path.cpp`를 공용 `re2dj_storage_common` 정적 라이브러리로 이동합니다.
2. `re2dj_chd_storage`가 새 라이브러리를 PUBLIC 링크하도록 CMake를 갱신합니다.
3. `re2dj_core`의 중복 소스 항목을 제거하고 아키텍처 문서의 라이브러리 표를 갱신합니다.
4. CHD 전용 4th 프로파일이 디렉터리 HDD scan fingerprint에 중복 매칭되지 않도록 보완합니다.
5. Debug x86 injected runtime target과 제품 executable의 링크를 검증합니다.
6. 결과와 재현 가능한 빌드/실행 점검을 작업 로그에 기록합니다.

### 제외 범위

CHD/FAT32 동작 변경, 원본 자산 수정, injected runtime API 동작 변경, 3rd hardlock 재개, 빌드 산출물 커밋은 포함하지 않습니다.

## English

### Goal

Fix the Debug x86 link failure that prevents `re2dj_windows_injected_runtime.dll` from being produced, so `re2dj.exe ez2dj4th --run` can find its bundled injected runtime.

### Scope

1. Move `src/storage/guest_path.cpp` into a shared `re2dj_storage_common` static library.
2. Link that library PUBLIC from `re2dj_chd_storage` in CMake.
3. Remove the duplicate source entry from `re2dj_core` and update the architecture library table.
4. Prevent the CHD-only 4th profile from being duplicated by directory-HDD fingerprint matching.
5. Verify the Debug x86 injected-runtime target and product executable links.
6. Record reproducible build and runtime-discovery checks in a work log.

### Out of scope

Do not change CHD/FAT32 behavior, original assets, injected-runtime API behavior, resume 3rd hardlock work, or commit build artifacts.
