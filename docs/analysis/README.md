# 분석 색인 / Analysis Index

이 디렉터리는 원본 EZ2DJ 바이너리와 HDD 자산에서 **직접 확인한** 프로젝트 고유 사실을 주제별로 누적한다. 일반 기술 배경은 [`docs/kb/`](../kb/README.md)에 둔다.

*This directory accumulates project-specific facts **verified directly** against the original EZ2DJ binaries and HDD assets, organized by topic. General background knowledge lives in [`docs/kb/`](../kb/README.md).*

## 표기 규칙 / Notation

모든 서술은 **확인됨 / 추정 / 미확정** 중 하나로 표기한다. 확인됨에는 검증 방법을, 추정에는 근거를, 미확정에는 확인 방법을 함께 적는다.

*Every statement is marked **confirmed**, **inferred**, or **unresolved**, alongside the verification method, the evidence, or the way to find out.*

## 문서 / Documents

| 문서 | 내용 | 현재 상태 |
| --- | --- | --- |
| [ez2dj-hdd-layout.md](ez2dj-hdd-layout.md) | HDD 덤프의 디렉터리 구조와 실행 파일 식별 | 1st SE / 3rd 덤프로 확인됨 |
| [ez2dj-import-surface.md](ez2dj-import-surface.md) | 원본이 실제로 호출하는 Win32 API 집합과 HLE 우선순위 | `ez2dj1.exe`로 확인됨 |

새 분석 문서를 추가하거나 이름을 바꾸면 같은 작업에서 이 표를 갱신한다.

*Update this table in the same task whenever an analysis document is added or renamed.*
