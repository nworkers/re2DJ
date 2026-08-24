# 논리 display mode HLE 작업 지시

관련 설계: [논리 display mode HLE](../design/20260825-059-logical-display-mode-hle.md)

## 목표

관찰된 640×480×16 guest display 요청을 host desktop 변경 없이 성공 처리하여 원본 초기화를 첫 파일 API까지 진행시킨다.

## 작업 범위

1. injected runtime에 strict-match `ChangeDisplaySettingsExA` wrapper를 추가한다.
2. launcher에 조합 가능한 `--hle-display-mode` 옵션과 USER32 IAT 교체를 추가한다.
3. runtime probe에 exact-match와 fallback 계약을 추가한다.
4. Windows x86 build와 CTest를 수행한다.
5. 정상 LPTDI state와 VFS를 결합한 canonical 실행을 최소 두 번 수행한다.
6. 첫 파일 API 또는 다음 차단점을 누적 분석·TODO·아키텍처·작업 로그에 반영하고 커밋한다.

## 완료 조건

host display를 바꾸지 않고 원본의 display check를 통과하며, 첫 파일 API와 경로를 반복 확인하거나 다음 차단점을 재현 가능한 caller와 인자로 확정한다.

## 수행 결과

구현과 probe 검증을 완료했다. 두 canonical 실행에서 display check를 통과했고, 다음 차단점을 `0x00422f39`의 null `IDirect3DDevice3` 정리 호출로 반복 확정했다. 선행 Direct3D 초기화의 실패 단계와 HRESULT 추적은 후속 작업으로 분리한다.

---

# Logical Display-Mode HLE Work Order

Related design: [Logical Display-Mode HLE](../design/20260825-059-logical-display-mode-hle.md)

## Goal

Accept the observed 640×480×16 guest display request without changing the host desktop and advance original initialization to the first file API.

## Scope

Add a strict-match injected-runtime wrapper, a composable `--hle-display-mode` launcher option and USER32 IAT replacement, exact-match/fallback runtime-probe coverage, Windows x86 build and CTest, at least two canonical LPTDI+VFS runs, cumulative documentation, work log, and commit.

## Completion criteria

Pass the display check without host mutation and repeatedly identify the first file API/path, or narrow the next blocker to reproducible caller and arguments.

## Execution result

Implementation and probe verification are complete. Two canonical runs pass the display check and repeatedly identify the next blocker as cleanup dereferencing a null `IDirect3DDevice3` at `0x00422f39`. Tracing the preceding Direct3D initialization stage and HRESULT is deferred to the next task.
