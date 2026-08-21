# 프로젝트 작업 규칙 설계

## 배경

re2DJ는 원본 EZ2DJ 실행 파일을 보존 실행하는 장기 작업이고, 게스트 형식과 호스트 범위가 rePIU와 다르다. 코드 변경보다 설계와 추적 가능성이 먼저 정리되어야 한다.

기존에 같은 접근으로 진행 중인 [rePIU](https://github.com/nworkers/rePIU)가 작업 규칙, 문서 구조, 코딩 스타일을 이미 정착시켰다. 두 프로젝트를 오가며 일하게 되므로 규칙 체계를 새로 만들기보다 이어받는 편이 낫다.

## 설계

작업 흐름은 요구사항 접수, 맥락 확인, 설계 문서 작성 또는 갱신, 작업 지시 작성 또는 갱신, 구현, 검증, 작업 로그 작성 순서로 고정한다.

문서는 한국어를 먼저 쓰고 영어 번역을 바로 아래에 추가한다. 소스 코드 주석은 영어 한 벌만 둔다. 실행 파일 분석 결과는 한국어/영어 문서를 분리해 누적한다.

플랫폼 공용 구조를 먼저 설계하고, Windows/Linux/Web 세부 구현은 플랫폼별 디렉터리로 분리한다.

## rePIU에서 이어받는 것

* 설계 우선 워크플로와 작업 지시·작업 로그 문서 단위
* 한국어 우선 이중 언어 문서 규칙과 영어 전용 소스 주석 규칙
* `docs/design/`, `docs/work-orders/`, `docs/work-logs/`, `docs/analysis/`, `docs/kb/`, `docs/guides/` 디렉터리 구조
* Google C++ Style Guide 기준에 중괄호·들여쓰기·명명 예외를 더한 코딩 스타일
* `VERSION` 파일 기반 버전 관리와 Git 브랜치·머지·태그 규칙
* BSD 3-Clause 라이선스 기준과 전염성 라이선스 배제
* 원본 자산 비포함 원칙

## re2DJ에서 달라지는 것

| 항목 | rePIU | re2DJ | 이유 |
| --- | --- | --- | --- |
| 플랫폼 디렉터리 | `src/platform/win32/` | `src/platform/{windows,linux,web}/` | Windows 호스트가 64비트이고 Linux·Web이 동등한 목표다 |
| 환경 경계 | DOS/DPMI interrupt | Win32 import thunk | 게스트가 Win32 PE32다 |
| 배제 대상 | DOSBox | Wine, QEMU, VirtualBox | 같은 문제 영역의 기존 구현이 다르다. Wine은 LGPL이라 라이선스 정책과도 충돌한다 |
| 자산 입력 | MAME ROM ZIP + CHD | HDD 디렉터리 경로 | 사용자가 요구한 입력 형태다 |
| 공용 코어 제약 | 명시 없음 | 호스트 OS 헤더 포함 금지 | 세 호스트를 동시에 지원해야 한다 |

추가로 "원본 바이너리에서 확인하지 않은 내용을 확정된 사실처럼 기술하지 않는다"를 핵심 원칙에 명시한다. 원본 덤프를 아직 확인하지 않은 상태에서 버전별 경로나 API를 추측해 문서에 넣으면, 나중에 그것이 사실처럼 인용된다.

## Background

re2DJ is a long-running effort to preserve and execute the original EZ2DJ executable, and its guest format and host scope differ from rePIU's. Design clarity and traceability must come before code changes.

[rePIU](https://github.com/nworkers/rePIU) already settled workflow rules, documentation structure, and coding style for the same approach. Since work will move between the two projects, inheriting that rule system beats inventing a second one.

## Design

The workflow is fixed as requirement intake, context inspection, design document creation or update, work-order creation or update, implementation, verification, and work-log creation.

Documents lead with Korean and add an English translation; source comments carry one English copy. Executable analysis findings accumulate in separate Korean and English documents.

Shared multiplatform structures are designed first, and Windows/Linux/Web details are separated into platform-specific directories.

## Inherited from rePIU

Design-first workflow with work-order and work-log units; Korean-first bilingual documents with English-only source comments; the `docs/` subdirectory structure; the Google C++ Style Guide baseline with brace, indentation, and naming exceptions; `VERSION`-based versioning with the Git branch, merge, and tag rules; the BSD 3-Clause baseline with copyleft excluded; and the rule that original assets never enter the repository.

## Changed for re2DJ

The platform directories gain Linux and Web and drop the bit width from the Windows name; the environment boundary becomes the Win32 import thunk; the excluded prior art becomes Wine, QEMU, and VirtualBox, with Wine also excluded on license grounds; asset input becomes an HDD directory path; and the shared core is barred from including host OS headers.

One principle is added: nothing is stated as confirmed fact unless it was verified against the original binary. With no dump inspected yet, a guessed path or API written into a document would later be cited as though it were established.
