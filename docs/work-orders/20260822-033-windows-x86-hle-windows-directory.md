# Windows x86 HLE GetWindowsDirectoryA 작업 지시

## 작업 내용

1. `re2dj.exe` 디렉터리의 `windows` support directory 정책을 문서화한다.
2. runtime `GetWindowsDirectoryA` HLE와 설정 buffer를 구현한다.
3. launcher가 absolute support path를 설정하고 IAT patch·제한 실행을 검증한다.

## English

1. Document the `windows` support-directory policy beside `re2dj.exe`.
2. Implement the runtime `GetWindowsDirectoryA` HLE and configuration buffer.
3. Have the launcher configure the absolute support path and verify the IAT patch in a limited run.
