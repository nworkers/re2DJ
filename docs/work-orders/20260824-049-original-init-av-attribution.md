# 원본 초기화 간접 호출 AV 귀속 작업 지시

관련 설계: [원본 초기화 간접 호출 AV 귀속](../design/20260824-049-original-init-av-attribution.md)

## 목표

작업 48 뒤 남은 `0x19d521bd` 실행 access violation의 호출 명령과 함수 포인터 저장소를 런타임 증거로 확정한다.

## 작업 범위

1. access violation diagnostic에 exception access kind/address와 전체 x86 register를 추가한다.
2. main image를 가리키는 register별 bounded dword window와 stack return 주소 주변 code bytes를 추가한다.
3. Windows x86 Debug build와 CTest를 실행한다.
4. `--hle-vfs --api-trace --device-mock-lptdi`를 최소 2회 실행해 안정성을 비교한다.
5. 결과를 executable structure/HDD analysis, TODO, 작업 로그에 반영하고 커밋한다.

## 해석 경계

비보호 형제 빌드의 정적 배열은 비교 기준일 뿐 canonical 보호 빌드의 런타임 사실이 아니다. fault context가 확인하기 전까지 손상된 initializer sentinel은 추정으로 유지한다.

## 검증

빌드·CTest 통과와 두 실행의 동일한 간접 호출 귀속으로 완료를 판정한다.

## 완료 상태

범위 1~5를 완료했다. 두 실행에서 손상된 `.data` initializer 첫 슬롯의 간접 호출로 동일하게 귀속했으며, IOCTL 실패와 불완전 복원의 인과는 후속 작업으로 분리했다. 결과는 [작업 로그 049](../work-logs/20260824-049-original-init-av-attribution.md)에 있다.

---

# Original-Initialization Indirect-Call AV Attribution Work Order

Related design: [Original-Initialization Indirect-Call AV Attribution](../design/20260824-049-original-init-av-attribution.md)

## Goal

Use runtime evidence to identify the calling instruction and function-pointer storage behind execute access violation `0x19d521bd` left after task 48.

## Scope

Add access metadata, full x86 registers, bounded main-image register windows, and code bytes around stack returns; build and run CTest; perform at least two canonical mock-on runs; update cumulative analysis, TODO, and a work log; commit the task.

## Interpretation boundary

The static array in the unprotected sibling is only a comparison baseline, not a runtime fact about the protected canonical build. The corrupted-initializer-sentinel attribution remains inferred until fault context confirms it.

## Verification

Completion requires a passing build/CTest and the same indirect-call attribution in two runs.

## Completion status

Scope items 1–5 are complete. Both runs attribute the fault to an indirect call through the corrupt first `.data` initializer slot. Causality between failed IOCTLs and incomplete restoration is separated into follow-up work. See [work log 049](../work-logs/20260824-049-original-init-av-attribution.md).
