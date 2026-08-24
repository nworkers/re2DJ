# 정상 초기화 후 첫 원본 자산 API 관찰 작업 지시

관련 설계: [정상 초기화 후 첫 원본 자산 API 관찰](../design/20260824-058-first-original-asset-api.md)

## 목표

정상 복원된 보호 실행 파일이 원본 `.text`에서 처음 호출하는 자산 파일 API와 guest 경로를 확인하고 VFS 연결의 다음 구현 범위를 확정한다.

## 작업 범위

1. 작업 57 정상 로그에서 원본 이미지 caller의 API 순서를 추출한다.
2. target state `0900000000000000`을 고정해 host baseline과 `--hle-vfs` 실행을 비교한다.
3. 첫 자산 API의 경로·caller·access·disposition과 후속 read/seek/size/close를 기록한다.
4. wrapper 안쪽 호출이 보이지 않으면 bounded runtime diagnostic을 설계·구현한다.
5. Windows x86 build와 CTest, 최소 두 번의 canonical 실행으로 검증한다.
6. TODO·누적 분석·아키텍처·작업 로그를 갱신하고 커밋한다.

## 완료 조건

첫 자산 파일 API와 VFS mapping을 반복 확인하거나, 자산 접근 전에 종료시키는 마지막 조건을 재현 가능한 caller와 인자로 확정한다.

## 수행 결과

두 정책 실행에서 자산 접근 전 마지막 조건을 640×480×16 `ChangeDisplaySettingsExA` 실패 분기로 확정했다. 첫 파일 API 관찰은 display-mode HLE 뒤 계속한다.

---

# First Original Asset API Work Order

Related design: [First Original Asset API After Stable Initialization](../design/20260824-058-first-original-asset-api.md)

## Goal

Identify the first asset-file API and guest path called from original `.text` after normal protected-image restoration, then define the next VFS integration scope.

## Scope

Extract original-image API callers from Task 57 logs; compare a fixed target-state host baseline and `--hle-vfs` run; record path, caller, access, disposition, and subsequent read/seek/size/close; add a bounded runtime diagnostic only if wrappers hide the boundary; verify Windows x86 build, CTest, and at least two canonical runs; update TODO, cumulative analysis, architecture, and work log; commit.

## Completion criteria

Repeatedly identify the first asset-file API and VFS mapping, or identify the final pre-asset termination condition with reproducible caller and arguments.

## Execution result

Two policy runs identify the final pre-asset condition as the failed 640×480×16 `ChangeDisplaySettingsExA` branch. First-file observation continues after display-mode HLE.
