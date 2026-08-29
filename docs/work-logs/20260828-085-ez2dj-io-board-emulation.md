# EZ2DJ I/O board 에뮬레이션 작업 로그

관련 설계: [EZ2DJ I/O board 에뮬레이션](../design/20260828-085-ez2dj-io-board-emulation.md)

관련 작업 지시: [EZ2DJ I/O board 에뮬레이션](../work-orders/20260828-085-ez2dj-io-board-emulation.md)

## 한국어

### 결과

- 2EZConfig-V2 commit `a7346e066bb569643e0cb25f41cfdc079126fbc6`에서 관찰한 port protocol을 원본 분석 사실과 분리해 기록했다. GPL-3.0 source나 dependency는 복사·링크하지 않았다.
- 플랫폼 중립 `Ez2DjIoBoard`가 21개 button, 두 개의 8-bit absolute turntable, coin state와 23개 active-high light를 소유한다. 당시 one-read coin latch로 구현했으나 작업 087의 실제 검증에서 counter 계약으로 정정됐다.
- `LegacyIoPortBus`는 기존 raw override/last-output API를 유지하면서 semantic board를 기본 구현으로 사용한다. 작업 087 이후 idle bytes는 `ff ff 80 80 00 ff`다.
- Windows keyboard adapter가 외부 INI를 한 번 읽고 `GetAsyncKeyState`로 button·coin·turntable 상태를 갱신한다. 제품 `--io-config`는 launcher에서 regular file과 absolute path를 검증한 뒤 injected runtime export로 전달된다.
- 예제는 [`config/ez2dj-io.example.ini`](../../config/ez2dj-io.example.ini)에 추가했다. 출력 light는 공용 semantic 조회 API로 노출했으며 실제 HID/network/cabinet sink는 후속 adapter 범위로 남겼다.

### 검증

- 격리된 `build/windows-x86-task085`에서 `RE2DJ_WARNINGS_AS_ERRORS=ON`, Win32 전체 build 성공. 기존 기본 build의 DLL은 권한이 다른 기존 `ez2dj.exe` PID 1888이 점유하여 같은 출력 경로의 재링크만 불가능했으므로 dependency source를 재사용한 격리 build로 검증했다.
- CTest 3/3 성공: runtime probe, product loader probe, unit tests.
- 당시 공용 unit tests는 419 checks, failures 0이었으며 입력 bank, turntable 위치, 초기 coin edge/consume 해석, light decode, raw override를 포함했다. coin 계약은 작업 087의 counter 회귀 테스트로 대체됐다.
- 제품 probe가 기본 omission과 `--io-config keyboard.ini` 두 argument 전달을 확인했다. runtime probe가 exported config path buffer transport를 확인했다.
- 실제 원본을 예제 INI와 함께 약 10초 실행했다. launcher log `20260828-203308-955.jsonl`은 I/O runtime prepared와 detached 상태를 기록했고, graphics trace는 sequence 786까지 성공한 main-loop draw를 기록했다. privileged-instruction, access-violation 또는 config parse 오류는 관찰되지 않았다. 자동 key press와 물리 light 출력 정확성은 사용자 장치 검증으로 남는다.

## English

### Result

The externally observable port protocol from 2EZConfig-V2 commit `a7346e0` is recorded separately from facts confirmed in the original executable; no GPL-3.0 source or dependency was copied or linked. Platform-neutral `Ez2DjIoBoard` owns semantic buttons, two absolute turntables, coin state, and active-high lights. Task 087 later corrects the original one-read coin latch to the counter contract confirmed through real input and original current-minus-previous calculation. `LegacyIoPortBus` adapts that state while retaining raw diagnostic compatibility.

Optional product `--io-config` carries a validated absolute INI path through the launcher into the injected runtime. The Windows adapter samples configured keys and supplies semantic input; omitted configuration preserves idle state. Light state is exposed for a later physical HID, network, or cabinet-output adapter.

### Verification

An isolated warnings-as-errors Windows x86 build linked the complete runtime successfully. CTest passed 3/3, and the common suite passed 419 checks with no failures. Product and runtime probes cover configuration argument/export transport. A roughly ten-second original 1st SE run using the example INI prepared and detached the I/O runtime, reached repeated successful main-loop draws through sequence 786, and produced no privileged-instruction, access-violation, or configuration-parse error. Automated key presses and physical lamp correctness remain user-device validation.
