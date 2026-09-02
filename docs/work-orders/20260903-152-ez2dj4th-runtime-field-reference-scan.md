# Task 152: EZ2DJ 4th 런타임 field 직접 참조 스캔

## 작업 목표

실행 중 복호화된 EZ2DJ 4th `.text`에서 `+0x11c` displacement를 직접 사용하는 instruction을 수집하여, `0x00acd708 + 0x11c` field의 직접 writer 존재 여부를 판별합니다.

## 구현 범위

1. 런타임 `.text` RVA와 virtual size 상수를 launcher probe에 추가합니다.
2. 첫 field access hit에서 원격 `.text`를 한 번 읽습니다.
3. ModRM disp32 `0x11c` 후보를 read/write/other로 분류하고 제한된 개별 이벤트와 요약 이벤트를 남깁니다.
4. 기존 source/access/early-writer trace 및 Hardlock/VFS 경로의 동작은 변경하지 않습니다.
5. 설계 문서, 누적 analysis, 작업 로그를 한국어·영어 순서로 갱신합니다.

## 제외 범위

- field 값 직접 주입
- Hardlock 응답 또는 seed 변경
- 원본 EXE, CHD, raw dump의 저장소 저장
- 후보 instruction에 대한 무제한 single-step 또는 전체 코드 에뮬레이션

## 완료 조건

- 스캔이 복호화된 런타임 `.text`에서 실행됩니다.
- 읽기 실패·부분 읽기를 성공적인 무참조 판정으로 오인하지 않습니다.
- 실제 `ez2dj4th` 실행 로그에 후보와 요약이 남습니다.
- Windows x86 Debug build와 전체 unit test가 통과합니다.
- 결과가 `docs/analysis/ez2dj4th-hardlock-runtime.md`와 대응 work log에 반영되고 커밋됩니다.

---

# Task 152: EZ2DJ 4th Runtime Direct Field-Reference Scan

## Objective

Determine whether a direct writer exists for field `0x00acd708 + 0x11c` by collecting instructions that use displacement `0x11c` in the decrypted EZ2DJ 4th runtime `.text`.

## Scope

1. Add the runtime `.text` RVA and virtual-size constants to the launcher probe.
2. Read the remote `.text` once on the first field-access hit.
3. Classify ModRM disp32 `0x11c` candidates as read/write/other and emit bounded candidate events plus a summary event.
4. Preserve the existing source/access/early-writer traces and Hardlock/VFS behavior.
5. Update the design document, cumulative analysis, and bilingual work log.

## Out of scope

- Directly injecting the field value.
- Changing Hardlock responses or seeds.
- Storing the original executable, CHD, or raw dumps in the repository.
- Unbounded single-stepping or whole-code emulation for candidates.

## Completion criteria

- The scan runs against decrypted runtime `.text`.
- Read or partial-read failure is not misclassified as a successful no-reference result.
- The real `ez2dj4th` execution log contains candidate and summary events.
- Windows x86 Debug build and the full unit-test suite pass.
- Results are reflected in `docs/analysis/ez2dj4th-hardlock-runtime.md` and the corresponding work log, then committed.
