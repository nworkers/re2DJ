# LPTDI/HASP 응답 계약 탐색 작업 지시

관련 설계: [LPTDI/HASP 응답 계약 탐색](../design/20260824-052-lptdi-hasp-response-contract.md)

## 목표

공개 HASP4 자료와 프로젝트 실행 증거를 구분해 문서화하고, LPTDI IOCTL의 preinitialized output을 유지하면서 full bytes-returned 성공 계약을 시험한다.

## 작업 범위

1. HASP4/Hardlock/Win32 IOCTL 배경 문서와 KB 색인을 추가한다.
2. runtime IOCTL mock에 zero/full-size mode export를 추가한다.
3. launcher에 `--device-mock-lptdi-ioctl-full-success` 선택 옵션과 diagnostic을 추가한다.
4. runtime probe에서 두 mode의 계약을 검증한다.
5. Windows x86 build·CTest와 canonical 2회 실행을 수행한다.
6. 결과를 analysis, architecture, TODO, 작업 로그에 반영하고 커밋한다.

## 완료 조건

기존 zero-byte mode가 회귀하지 않고 full-size mode 계약 테스트가 통과하며, canonical 2회 결과가 같은 제어 흐름 분류를 보여야 한다.

## 완료

KB, runtime mode, launcher 옵션과 probe 검증을 구현했다. Windows x86 build와 CTest 2/2가 통과했고 canonical 두 실행은 모두 full-size만으로는 원본 entry에 도달하지 못하고 private-page #UD 경로를 선택했다.

---

# LPTDI/HASP Response-Contract Exploration Work Order

Related design: [LPTDI/HASP Response-Contract Exploration](../design/20260824-052-lptdi-hasp-response-contract.md)

## Goal

Separate public HASP4 background from project runtime evidence, then test full bytes-returned success while preserving LPTDI's preinitialized output.

## Scope

Add indexed HASP4/Hardlock/Win32 IOCTL background, export zero/full runtime mock modes, add a selective launcher option and diagnostics, cover both contracts in the runtime probe, run Windows x86 build/CTest and two canonical executions, update cumulative documents, and commit.

## Completion criteria

The existing zero-byte mode must remain intact, the full-size contract probe must pass, and two canonical runs must produce the same control-flow classification.

## Completion

Implemented the knowledge-base entry, runtime mode, launcher option, and probe coverage. The Windows x86 build and CTest 2/2 passed, and both canonical runs showed that full-size reporting alone still selects the private-page #UD path before the original entry.
