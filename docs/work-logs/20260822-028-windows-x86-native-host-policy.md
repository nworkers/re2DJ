# Windows x86 Native Host Policy Result

## 한국어

Windows x86 full-host preset `windows-x86-debug`을 추가하고 Windows build/test script 기본값을 이 preset으로 전환했습니다. x64 Windows helper 및 original-process probe는 삭제하지 않고 보류된 실험 경로로 유지합니다.

검증 결과:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` 성공
2. `cmake --build --preset windows-x86-debug` 성공
3. `ctest --preset windows-x86-debug` — 1/1 통과
4. x86 `re2dj.exe --help` 실행 확인

다음 구현은 x86 host가 original EXE를 주 이미지로 시작하고 same-process runtime을 준비하는 launcher 설계입니다. x64 Windows 지원 재개 시에는 별도 backend로 다시 설계합니다.

## English

The Windows x86 full-host preset `windows-x86-debug` was added and Windows build/test script defaults now use it. The x64 Windows helper and original-process probes remain as deferred experimental paths rather than being deleted.

Verification:

1. `cmake --preset windows-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` succeeded.
2. `cmake --build --preset windows-x86-debug` succeeded.
3. `ctest --preset windows-x86-debug` — 1/1 passed.
4. The x86 `re2dj.exe --help` executable ran successfully.

The next implementation designs a launcher in which the x86 host starts the original EXE as main image and prepares an in-process runtime. Resuming x64 Windows support requires a separately designed backend.
