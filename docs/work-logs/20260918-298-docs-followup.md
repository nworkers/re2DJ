# 작업 298 작업 로그 — 작업 291~297 누적 문서 보완 / Task 298 work log — Filling in cumulative documents for tasks 291-297

작업 지시: [20260918-298-docs-followup.md](../work-orders/20260918-298-docs-followup.md)

## 한국어

### 대조 결과

v0.0.47(`157e8f1`)에는 코드, 테스트, 작업 295~297의 설계·지시·로그, `ARCHITECTURE.md`, 실행 가이드, 서드파티 고지, `IMPLEMENTED.md`, 분석 문서와 색인, 릴리스 노트가 들어 있었다. 빠진 것은 다음이었다.

| 누락 | 근거 규칙 | 조치 |
| --- | --- | --- |
| `EXE_DESIGN.ko.md`·`.en.md`에 작업 291~297의 원본 분석 결과가 없음 | "원본 파일 분석으로 확인한 구조와 설계는 누적 반영" | 확인됨/추정/미확정을 구분한 절 추가 |
| 작업 297에서 쓴 Win32 동작의 kb 문서 없음 | "새로운 API·동작이 등장하면 kb 작성" | `win32-cursor-and-child-window-input.md`와 색인 |
| 작업 297 설계에 해소된 미확정 항목(GL 2.1 VAO)이 남음 | 문서와 확인 상태 일치 | 해소로 표시 |
| 작업 291~297의 후속 항목이 TODO에 없음 | — | 다섯 항목 추가 |

### 검증

* 코드 변경 없음. 추가한 문서의 링크 경로를 저장소 기준으로 확인했다.

## English

### What the check found

v0.0.47 (`157e8f1`) carried the code, tests, the designs, work orders and logs of tasks 295-297, `ARCHITECTURE.md`, the runtime guide, third-party notices, `IMPLEMENTED.md`, the analysis documents and index, and release notes. Missing were: the original-analysis results of tasks 291-297 in `EXE_DESIGN.ko.md` and `.en.md`, required by the rule to accumulate confirmed structure there, now added with confirmed/inferred/unresolved marks; a kb topic for the Win32 behavior task 297 relied on, now `win32-cursor-and-child-window-input.md` with its index entry; task 297's design still listing the GL 2.1 VAO question as open, now marked resolved; and the follow-ups from tasks 291-297 in the TODO, now five entries.

### Verification

* No code change. The link paths of the added documents were checked against the repository.
