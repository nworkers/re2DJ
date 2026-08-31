# ez2dj3rd Hardlock Function 0x0E 분석 설계

## 목적

3rd 원본 실행 파일이 Hardlock 보호 경계를 통과하는 데 필요한 장치 이름, API descriptor, `Function 0x0e` 암호 응답 계약을 분리해서 확인합니다. 1st SE의 LPTDI 응답을 추측해서 재사용하지 않고, 원본 실행 흐름에서 확인된 요청과 아직 필요한 외부 입력을 구분합니다.

*Analyze the device name, API descriptor, and `Function 0x0e` cryptographic response contract required by the 3rd original executable. Do not guess and reuse the 1st SE LPTDI response; separate the requests confirmed from the original execution flow from the external inputs that are still required.*

## 현재 확인 사실

- **확인됨:** 3rd `EZ2DJ.EXE`에는 `KERNEL32.dll!GetProcAddress` IAT 슬롯이 두 개이며, 보호 entry stub가 사용하는 두 슬롯 모두를 runtime resolver로 연결해야 합니다.
- **확인됨:** all-slot 연결 후 실제 runtime trace에 `CreateFileA("\\.\\NTICE")`와 `CreateFileA("\\.\\FEnteDev")`가 기록되었습니다. `FEnteDev`는 이 실행에서 Hardlock 계층이 요청한 장치 이름입니다. `NTICE`의 정확한 역할은 아직 미확정이며 anti-debug 장치 조회 후보로만 기록합니다.
- **확인됨:** `FEnteDev`를 synthetic handle로 연결한 계측 실행에서 `0x9c402468`, `0x9c402450`, `0x9c40244c`, `0x9c402458` 순서의 요청이 관찰되었습니다.
- **확인됨:** `0x9c40244c`는 256바이트 in-place descriptor이고, descriptor의 API version은 `0x4703`, 초기 `Function`은 `0`, 이후 `Function 6` 요청이 관찰되었습니다.
- **확인됨:** `0x9c402450`은 6바이트 입출력 버퍼와 `FA FA` marker를 검사하는 하위 장치 응답 경계입니다.
- **확인됨:** `0x9c402458`은 264바이트 입출력 버퍼이며, 앞 256바이트 descriptor의 `Function`은 `0x0e`, 뒤 8바이트는 호출마다 달라지는 암호 블록입니다.
- **미확정:** 3rd의 실제 `Function 0x0e` 8바이트 응답 또는 그 응답을 만드는 세 개의 16비트 Hardlock seed입니다. zero target state와 1st SE의 LPTDI mask 변환은 이 응답을 대신하지 않습니다.

* **Confirmed:** The 3rd `EZ2DJ.EXE` contains two `KERNEL32.dll!GetProcAddress` IAT slots, and both slots used by the protected entry path must be routed to the runtime resolver.
* **Confirmed:** After all-slot routing, the runtime trace records `CreateFileA("\\.\\NTICE")` and `CreateFileA("\\.\\FEnteDev")`. `FEnteDev` is the device name requested by the Hardlock layer in this run. The exact role of `NTICE` remains unresolved and is recorded only as a possible anti-debug device query.
* **Confirmed:** With `FEnteDev` connected to a synthetic handle, instrumentation observed requests for `0x9c402468`, `0x9c402450`, `0x9c40244c`, and `0x9c402458`.
* **Confirmed:** `0x9c40244c` is a 256-byte in-place descriptor. Its API version is `0x4703`; the initial `Function` is `0`, followed by observed `Function 6` requests.
* **Confirmed:** `0x9c402450` is a lower-level device-response boundary with a six-byte input/output buffer and an `FA FA` marker check.
* **Confirmed:** `0x9c402458` uses a 264-byte input/output buffer. The first 256 bytes have `Function 0x0e`; the final eight bytes are an encrypted block that changes per call.
* **Unresolved:** The valid 3rd `Function 0x0e` eight-byte response, or the three 16-bit Hardlock seeds that generate it. The zero target state and the 1st SE LPTDI mask transform cannot substitute for this response.

## 구현 범위

1. 프로파일의 synthetic device path를 실제 3rd 요청 이름인 `\\.\\FEnteDev`로 정정합니다. `NTICE`는 보호 코드의 별도 조회이므로 자동 성공시키지 않습니다.
2. 모든 동일 이름 `GetProcAddress` IAT 슬롯을 연결하는 공용 PE helper와 launcher 변경을 유지합니다.
3. 현재 `TargetLptdiPolicy`의 zero state를 유효한 3rd Hardlock seed 또는 `Function 0x0e` 응답으로 승격하지 않습니다.
4. 유효한 0x0e 응답이 제공될 때까지 원본 코드가 임의의 성공 응답으로 진행하도록 만들지 않습니다.

*Implementation scope: (1) correct the profile's synthetic device path to the actual 3rd request name `\\.\\FEnteDev`; do not automatically succeed the separate `NTICE` query; (2) retain the shared PE helper and launcher change that routes every matching `GetProcAddress` IAT slot; (3) do not promote the current `TargetLptdiPolicy` zero state to a valid 3rd Hardlock seed or `Function 0x0e` response; and (4) do not make the original code proceed on an arbitrary success response until a valid 0x0e response is available.*

## 검증 전략

- Windows x86 Debug build와 CTest를 실행합니다.
- 실제 `re2dj ez2dj3rd` bounded 실행(`20260831-000859-972.jsonl`)에서 `device_mock_dynamic_resolver_slots=2`와 `FEnteDev` device-open trace를 확인합니다.
- 0x0e 유효 응답은 게임 화면 도달 또는 원본 보호 코드의 정상 후속 실행을 관찰하기 전까지 통과로 판정하지 않습니다.
- 다음 단계에 필요한 입력은 합법적으로 보유한 3rd Hardlock dump, 알려진 0x0e 입출력 응답 쌍, 또는 seed 값입니다. 원본 자산은 저장소에 추가하지 않습니다.

*Verification: run the Windows x86 Debug build and CTest; confirm `device_mock_dynamic_resolver_slots=2` and the `FEnteDev` device-open trace in a bounded `re2dj ez2dj3rd` run; do not call 0x0e valid until the original protection code proceeds normally or the game reaches its screen; and request a legally owned 3rd Hardlock dump, known 0x0e input/output pair, or seed values as the next required input. Original assets must not be added to the repository.*
