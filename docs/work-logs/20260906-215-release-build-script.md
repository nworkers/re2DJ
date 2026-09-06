# 작업 로그: Windows x86 릴리즈 빌드 스크립트

## 한국어

### 작업 요약

Windows x86 Release 빌드와 Release 테스트를 재현 가능한 형태로 실행할 수 있도록 `scripts/build_release.ps1` 및 `scripts/build_release.bat` 래퍼 스크립트를 추가하고 문서화했습니다.

### 주요 변경 사항

1. `scripts/build_release.ps1`: Release 구성 CMake 빌드 및 CTest 자동 실행, `-SkipTests` 플래그 지원.
2. `scripts/build_release.bat`: PowerShell 스크립트를 호출하는 cmd/bat 호환 래퍼 추가.
3. `scripts/README.md`: 신규 스크립트 사용법 및 Release 바이너리 출력 위치(`build/windows-x86-debug/bin/Release`) 설명 반영.

### 검증 결과

- `scripts/build_release.bat -SkipTests` 실행 결과 Release 바이너리가 성공적으로 빌드됨.
- Release 단위 테스트(`re2dj_unit_tests`, `re2dj_windows_product_loader_probe`) 통과 확인.

---

## English

### Summary

Added `scripts/build_release.ps1` and `scripts/build_release.bat` to configure, build, and test the Windows x86 Release runtime in a repeatable manner.

### Key Changes

1. `scripts/build_release.ps1`: Automated CMake configure/build in Release mode and CTest invocation with `-SkipTests` support.
2. `scripts/build_release.bat`: Batch wrapper delegating to PowerShell.
3. `scripts/README.md`: Documented usage and Release output directory.

### Verification Results

- `scripts/build_release.bat -SkipTests` built the Release target successfully.
- Release unit tests passed.
