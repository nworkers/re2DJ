# 원본 controlled exit 원인 귀속 작업 로그

관련 설계: [controlled exit 원인 귀속](../design/20260825-063-controlled-exit-attribution.md)

관련 작업 지시: [controlled exit 원인 귀속 작업 지시](../work-orders/20260825-063-controlled-exit-attribution.md)

## 결과

`ExitProcess`의 직접 return `0x00424061`은 오류 원인이 아니라 54개 원본 오류 경로가 공유하는 helper `0x00424040` 내부 주소임을 확인했다. launcher가 이 return과 EBP frame을 함께 검증해 실제 caller, format 문자열과 첫 detail 인자를 기록하도록 확장했다.

최종 두 실행의 귀속 결과는 동일하다.

| 항목 | 값 |
| --- | --- |
| caller | `0x00424813` |
| format pointer | `0x004570ec` |
| format | `KSND(ksndLoadSound) : failed to load %s` |
| detail pointer | `0x004581e8` |
| detail | `coin0.wav` |

정적 caller `0x00424740`은 입력 파일명에서 확장자를 분리하고 KSND search-path lookup `0x00423f70`을 호출한다. 반환값이 1이면 위 오류 helper를 호출한다. HDD를 읽기 전용 확인한 결과 실제 파일은 사용자 지정 root 아래 `ez2dj/System/Common/coin0.wav`에 존재한다. search-path count가 0인지, 생성된 후보 path가 VFS에서 잘못 변환되는지는 아직 확인하지 않았다.

## 구현과 검증

- 확인된 wrapper return RVA `0x24061`에서만 EBP frame 해석
- frame read, main-image caller, format/detail string validity를 각각 기록
- invalid frame은 guest 흐름을 변경하지 않는 fail-closed 진단
- Windows x86 Debug build 성공
- Windows x86 CTest 2/2 통과
- 최종 canonical 로그:
  - `20260825-014625-442.jsonl`
  - `20260825-014719-632.jsonl`

두 로그 모두 동일한 caller와 문자열을 기록하며 `av_access`는 없다. 원본 HDD와 실행 파일은 변경하지 않았다.

## 다음 작업

KSND search-path global count와 등록 entry를 종료 시점에 관찰하고, `Re2djVfsCreateFileA`에 bounded 원본/변환 path 결과를 추가해 `coin0.wav`가 `System/Common` 후보로 실제 조회되는지 확인한다. 이는 먼저 별도 설계로 정리하고 임의의 파일 fallback은 추가하지 않는다.

---

# Controlled Exit Cause Attribution Work Log

Related design: [Controlled Exit Cause Attribution](../design/20260825-063-controlled-exit-attribution.md)

Related work order: [Controlled Exit Cause Attribution Work Order](../work-orders/20260825-063-controlled-exit-attribution.md)

## Result and verification

ExitProcess return 0x00424061 belongs to shared helper 0x00424040 rather than identifying one of its 54 error callers. The launcher now validates that wrapper and reads its EBP frame without changing guest flow. Windows x86 Debug builds and CTest passes 2/2.

Final logs `20260825-014625-442.jsonl` and `20260825-014719-632.jsonl` both attribute the exit to caller 0x00424813 with format `KSND(ksndLoadSound) : failed to load %s` and detail `coin0.wav`; neither contains an access violation. The supplied HDD contains `ez2dj/System/Common/coin0.wav`, but search-path registration and concrete CreateFile candidates remain unresolved. The next task will observe those values before changing any VFS policy.
