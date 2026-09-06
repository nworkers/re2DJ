# 작업 지시서: EZ2DJ 3rd ReadFile 경계 진단

## 한국어

### 배경

`ez2dj3rd` 실행 로그는 초기 `FEnteDev`, `EZ2DJ.ini`, 폰트 파일 접근까지 확인하지만 실행이 화면에 도달하지 않거나 Hardlock transform 단계 이전에 대기합니다. 현재 파일 열기 로그만으로는 파일 읽기 내부 대기와 이후 경계 대기를 구분할 수 없습니다.

### 작업

1. 기존 VFS/Hardlock 설계와 3rd 분석 기록을 기준으로 계측 범위를 고정합니다.
2. `ReadFile` 진입과 반환을 bounded VFS trace에 추가합니다.
3. native, CHD, device-mock 경로를 동일하게 검증합니다.
4. Windows x86 Debug 빌드와 기존 단위 테스트를 실행합니다.
5. `ez2dj3rd`를 제한 시간 동안 실행하여 새 로그의 마지막 파일 경계를 확인합니다.
6. 결과와 미확정 사항을 작업 로그와 관련 구조 문서에 기록합니다.

### 제외 범위

- Hardlock challenge/response/seed 추정 또는 변경
- `cfg/hardlock-ez2dj3rd.map` 변경
- 원본 HDD, CHD, 실행 파일의 저장소 복사
- 그래픽 또는 blending 동작 변경

### 검증 계획

- `scripts/build_win32.bat`
- `ctest --test-dir build/windows-x86 -C Debug -R "re2dj_unit_tests|re2dj_windows_product_loader_probe" --output-on-failure`
- 사용자와 동일한 `ez2dj3rd --io-config` 실행을 제한 시간 동안 수행
- 생성된 `.vfs.log`에서 `read-file-enter`/`read-file-result` 대응과 마지막 이벤트 확인
- legacy I/O가 처리하지 못한 `EXCEPTION_PRIV_INSTRUCTION`이 있으면 기존 crash-context 형식으로 첫 fault를 기록하되, I/O를 활성화하지 않음

## English

### Background

The `ez2dj3rd` run reaches the initial `FEnteDev`, `EZ2DJ.ini`, and font-file accesses, but it does not reach the screen or waits before the Hardlock transform stage. The current open-file trace cannot distinguish a wait inside a file read from a wait at a later boundary.

### Work

1. Fix the instrumentation scope using the existing VFS/Hardlock design and 3rd analysis records.
2. Add bounded VFS records for `ReadFile` entry and return.
3. Verify native, CHD, and device-mock paths consistently.
4. Build the Windows x86 Debug target and run the existing focused tests.
5. Run `ez2dj3rd` for a bounded interval and inspect the last file-boundary records.
6. Record the result and unresolved items in the work log and relevant architecture documentation.

### Out of scope

- Guessing or changing Hardlock challenge/response/seed material
- Changing `cfg/hardlock-ez2dj3rd.map`
- Copying original HDD, CHD, or executable assets into the repository
- Changing graphics or blending behavior

### Verification plan

- Run `scripts/build_win32.bat`.
- Run the focused unit and product-loader tests.
- Run the same `ez2dj3rd --io-config` command as the user for a bounded interval.
- Match `read-file-enter` and `read-file-result` events and identify the final event in the generated `.vfs.log`.
- If legacy I/O cannot claim an `EXCEPTION_PRIV_INSTRUCTION`, record the first fault using the existing crash-context format without enabling I/O handling.
