# 작업 지시서: 16비트 폭 legacy I/O 경계

## 한국어

### 관련 설계

[16비트 폭 legacy I/O 경계 설계](../design/20260910-244-word-width-legacy-io.md)

### 작업 항목

1. `TargetLptdiPolicy`에 `LegacyIoWidth`와 `legacy_io_width`를 추가합니다. 기본값은 `kByte`입니다.
2. `legacy_io_in_byte_rva`·`legacy_io_out_byte_rva`를 `legacy_io_in_rva`·`legacy_io_out_rva`로 바꿉니다. injected runtime의 export 이름은 바꾸지 않습니다.
3. `Ez2DancerIoBoard`를 전용 header와 source로 추가합니다.
4. word 폭 API를 갖는 `Ez2DancerIoPortBus`를 추가합니다. `LegacyIoPortBus`는 손대지 않습니다.
5. privileged fault handler가 `0x66` prefix를 해석해 opcode·명령 길이·폭을 정하고, 폭에 맞는 bus로 보내며, `EIP`를 명령 길이만큼 진행시키게 합니다.
6. 읽기 결과를 폭에 맞게 `EAX`에 반영합니다. byte는 하위 8비트, word는 하위 16비트만 바꿉니다.
7. 프로파일이 선언한 폭과 어긋나는 opcode는 처리하지 않고 crash로 보고합니다.
8. launcher가 폭을 runtime에 전달하게 합니다.
9. `ez2d2m` 프로파일의 raw I/O를 켜고 `legacy_io_width`와 `legacy_io_out_rva`를 설정합니다.
10. 단위 시험을 추가합니다. prefix 해석, 폭별 `EAX` 반영, EZ2Dancer port 의미를 덮습니다.
11. Windows x86 build와 CTest를 검증합니다.
12. `ez2d2m`이 정지 지점을 넘기는지 실행으로 확인합니다.
13. 기존 다섯 제품의 byte 경로에 회귀가 없는지 확인합니다.
14. 분석 문서와 작업 로그를 갱신합니다.

### 제외 범위

- 32비트 폭 port 접근
- EZ2Dancer 키보드 입력 매핑
- 원본에서 확인되지 않은 bit 배치의 확정
- `ez2d2m`의 실행 성공 보장

### 완료 조건

- `ez2d2m`이 RVA `0x0000b565`의 `out dx, ax`를 트랩하고 그 다음으로 진행합니다.
- 기존 다섯 제품의 프로파일 값과 실행 경로가 변하지 않습니다.
- 단위 시험이 통과하고 Windows x86 build가 경고 없이 통과합니다.

## English

### Related design

[Word-width legacy I/O boundary design](../design/20260910-244-word-width-legacy-io.md)

### Work items

1. Add `LegacyIoWidth` and `legacy_io_width` to `TargetLptdiPolicy`, defaulting to `kByte`.
2. Rename `legacy_io_in_byte_rva` and `legacy_io_out_byte_rva` to `legacy_io_in_rva` and `legacy_io_out_rva`, leaving the injected runtime's exported symbol names alone.
3. Add `Ez2DancerIoBoard` in its own header and source.
4. Add `Ez2DancerIoPortBus` with a word-width API, leaving `LegacyIoPortBus` untouched.
5. Make the privileged-fault handler decode a `0x66` prefix into opcode, instruction length and width, dispatch to the bus for that width, and advance `EIP` by the decoded length.
6. Apply a read result according to width: byte replaces the low 8 bits of `EAX`, word the low 16.
7. Report an opcode disagreeing with the profile's declared width as a crash rather than handling it.
8. Have the launcher pass the width to the runtime.
9. Turn raw I/O on for the `ez2d2m` profile with its width and output helper RVA.
10. Add unit tests covering prefix decoding, the per-width `EAX` update, and the EZ2Dancer port meanings.
11. Verify the Windows x86 build and CTest.
12. Confirm by running that `ez2d2m` gets past the stop.
13. Confirm no regression in the five existing products' byte path.
14. Update the analysis documents and write the work log.

### Out of scope

- 32-bit-wide port access
- An EZ2Dancer keyboard mapping
- Settling bit layouts the original has not confirmed
- Guaranteeing that `ez2d2m` runs

### Completion criteria

- `ez2d2m` traps the `out dx, ax` at RVA `0x0000b565` and proceeds past it.
- The five existing products' profile values and execution path are unchanged.
- Unit tests pass and the Windows x86 build passes without warnings.
