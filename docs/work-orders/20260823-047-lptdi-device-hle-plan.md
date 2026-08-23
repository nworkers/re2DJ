# 병렬포트 디바이스 검사 HLE 계획 작업 지시

관련 설계: [병렬포트 디바이스 검사 HLE 계획](../design/20260823-047-lptdi-device-hle-plan.md)

## 목표

보호 스텁의 `\\.\LPTDI*` 개방 실패가 종료로 이어지는 인과를 검증하고, Win32 파일 API 경계에서 디바이스를 에뮬레이션하는 단계별 계획을 실행 가능한 상태로 만든다.

## Phase A 작업 (첫 구현 단위)

1. launcher probe의 watched API에 `DeviceIoControl`, `ReadFile`, `WriteFile`, `CloseHandle`을 추가한다(기존 4-인자 기록으로 IOCTL 코드까지 수집).
2. canonical `ez2dj.exe`를 실행해 개방 실패 전후의 디바이스 핸들 사용 행적(IOCTL·버퍼 크기·반복)을 확정하고 분석 문서에 기록한다.
3. 결과에 따라 Phase B 가상 디바이스 테이블 설계를 갱신한다.

Phase B·C는 별도 작업 지시로 분리한다.

## 검증

Windows x86 build·CTest 통과와 새 API 기록의 생성을 판정 기준으로 한다.

---

# Parallel-Port Device Check HLE Plan Work Order

Related design: [Parallel-Port Device Check HLE Plan](../design/20260823-047-lptdi-device-hle-plan.md)

## Goal

Verify the causal link between the failed `\\.\LPTDI*` open and the termination, and make the phased device-emulation plan executable.

## Phase A tasks (first implementation unit)

1. Extend the launcher probe watch list with `DeviceIoControl`, `ReadFile`, `WriteFile`, and `CloseHandle`; the existing four-argument logging captures IOCTL codes.
2. Run canonical `ez2dj.exe`, pin the post-open handle usage (IOCTLs, buffer sizes, repetition), and record it in the analysis documents.
3. Update the Phase B virtual-device table design from the findings.

Phases B and C get their own work orders.

## Verification

Passing the Windows x86 build and CTest plus the appearance of the new API records in the diagnostic log.
