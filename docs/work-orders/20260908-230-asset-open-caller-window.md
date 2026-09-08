# 작업 지시서: 자산 열기 호출 지점과 코드 창

## 한국어

### 관련 설계

[자산 열기 호출 지점과 코드 창 설계](../design/20260908-230-asset-open-caller-window.md)

### 작업 항목

1. `Re2djVfsCreateFileA`가 `_ReturnAddress()`로 호출 지점을 얻습니다.
2. 자산 열기 trace에 호출 지점 주소와 image base 기준 RVA를 기록합니다.
3. 첫 번째 `.bmp` 열기에서 반환 주소 앞 32바이트·뒤 96바이트를 한 번 기록합니다.
4. Windows x86 build와 시험을 검증합니다.
5. 1st SE 실행으로 창을 수집하고 호출 직후 분기를 읽습니다.
6. 3rd·4th 회귀를 확인합니다.
7. 작업 로그에 읽어낸 분기와 결론을 남깁니다.

### 제외 범위

- 명령어 단위 단일 스텝 추적 추가
- 게스트 코드 수정
- 자산 적재 경로 구현 변경

### 완료 조건

- trace에 `.bmp` 열기의 호출 지점 RVA가 남습니다.
- 코드 창이 한 번 기록되고 바이트가 읽힙니다.
- 남은 후보가 구체화되거나 배제됩니다.
- unit test와 VFS runtime probe가 통과하고 3rd·4th에 회귀가 없습니다.

## English

### Related design

[Asset-Open Call Site and Code Window Design](../design/20260908-230-asset-open-caller-window.md)

### Work items

1. Take the call site in `Re2djVfsCreateFileA` from `_ReturnAddress()`.
2. Record that address and its image-base-relative RVA on the asset-open trace.
3. Record a window of 32 bytes before and 96 bytes after the return address once, on the first `.bmp` open.
4. Verify the Windows x86 build and tests.
5. Collect the window from a 1st SE run and read the branch after the call.
6. Confirm no regression for 3rd and 4th.
7. Record the branch read and the conclusion in the work log.

### Out of scope

- Adding instruction-level single-step tracing
- Modifying guest code
- Changing the asset-loading implementation

### Completion criteria

- The trace carries the call-site RVA for `.bmp` opens.
- One code window is recorded and its bytes are readable.
- The remaining candidate is given a concrete shape or eliminated.
- Unit tests and the VFS runtime probe pass with no 3rd or 4th regression.
