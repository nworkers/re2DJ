# Win32 제품 loader 통합 작업 지시

## 상태

**완료.** [Win32 제품 loader 통합 설계](../design/20260828-079-win32-product-loader.md)에 따라 구현과 검증을 마쳤다.

## 작업

1. 기존 launcher core를 `re2dj_windows_original_process_backend` static library로 승격한다.
2. 진단용 launcher executable을 shared engine을 호출하는 얇은 entry로 바꾼다.
3. filesystem path와 target ID를 받는 Windows product facade와 asset-free policy test를 추가한다.
4. 일반 CLI의 Windows `--run`을 facade에 연결하고 help/error 문구를 갱신한다.
5. Win32 warnings-as-errors build, CTest와 CLI help/unsupported-target 검증을 수행한다.
6. architecture, README, Windows 실행 guide, IMPLEMENTED와 작업 로그를 갱신한다.
7. 작업 변경을 커밋한다.

## 완료 기준

- 기존 launcher probe와 일반 `re2dj.exe`가 같은 original-process engine을 사용한다.
- `re2dj --hdd <dir> --target ez2dj1stse --run`이 canonical detached HLE policy로 진입한다.
- 미검증 target은 원본 process를 만들기 전에 명시적으로 거절된다.
- 기존 진단 launcher option과 Win32 CTest가 회귀하지 않는다.

---

# Win32 Product Loader Integration Work Order

## Status

**Complete.** Implementation and verification are complete according to the [Win32 product loader integration design](../design/20260828-079-win32-product-loader.md).

## Tasks

1. Promote the launcher core into a `re2dj_windows_original_process_backend` static library.
2. Replace the diagnostic launcher executable with a thin entry calling the shared engine.
3. Add a Windows product facade accepting a filesystem path and target ID plus asset-free policy tests.
4. Connect ordinary Windows `--run` to the facade and update help and error text.
5. Run the warnings-as-errors Win32 build, CTest, and CLI help/unsupported-target checks.
6. Update architecture, README, the Windows execution guide, IMPLEMENTED, and the work log.
7. Commit the task changes.

## Completion Criteria

The launcher probe and ordinary `re2dj.exe` share one original-process engine. `re2dj --hdd <dir> --target ez2dj1stse --run` enters the canonical detached HLE policy, unverified targets are rejected before process creation, diagnostic launcher options remain compatible, and Win32 CTest stays green.
