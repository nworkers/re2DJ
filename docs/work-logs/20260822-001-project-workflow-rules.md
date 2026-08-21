# 작업 로그: 프로젝트 작업 규칙 정리

## 결과

완료.

`AGENTS.md`에 프로젝트 개요, 핵심 설계 목표, 참조 문서, 핵심 원칙, 응답 태도, 요구사항 처리 절차, 문서 작성 규칙, 분석·지식 기반 유지 규칙, 원본 자산 취급 규칙, 구현 규칙, 작업 단위 규칙, Git 작업 규칙, 아키텍처 규칙, 개발 철학, 법적 범위를 반영했다.

관련 기준 문서로 `docs/PROJECT_CHARTER.md`, `docs/CODING_STYLE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`를 추가했다.

`docs/analysis/README.md`, `docs/kb/README.md`, `docs/guides/README.md` 색인을 추가했다.

`LICENSE`(BSD 3-Clause)와 `THIRD_PARTY_NOTICES.md`를 추가했다. rePIU는 라이선스 정책만 두고 `LICENSE` 파일이 없어 재배포 조건이 불명확한 상태였다. 같은 상태를 재현하지 않도록 처음부터 파일을 두었다.

`VERSION`을 `0.0.1`로 두었다.

## rePIU에서 조정한 부분

* 플랫폼 디렉터리를 `src/platform/{windows,linux,web}/`로 바꿨다. Windows 호스트가 64비트이므로 `win32`라는 이름은 오해를 부른다.
* 환경 경계를 DOS/DPMI interrupt에서 Win32 import thunk로 바꿨다.
* 배제 대상에 Wine을 추가했다. LGPL이므로 라이선스 정책과도 충돌한다는 근거를 함께 적었다.
* 원본 자산 취급 규칙을 별도 절로 분리했다. HDD 디렉터리 입력이 이 프로젝트의 자산 경로 전부이기 때문이다.
* 공용 코어에서 호스트 OS 헤더 포함을 금지하는 규칙을 추가했다.
* "확인하지 않은 내용을 확정된 사실처럼 기술하지 않는다"를 핵심 원칙에 추가했다.

## 검증

`find . -name '*.md'`로 문서 생성을 확인했다.

`AGENTS.md`의 구현 규칙이 지정한 `src/platform/{windows,linux,web}/`가 실제로 존재하며 각 디렉터리에 목적을 적은 `README.md`가 있음을 확인했다.

라이선스 정책(BSD 3-Clause 기준, 전염성 라이선스 배제)과 `LICENSE` 파일 내용(BSD 3-Clause), `THIRD_PARTY_NOTICES.md`의 서술이 서로 일치함을 확인했다.

이 저장소는 아직 Git 저장소가 아니므로 브랜치 확인은 수행하지 않았다. 초기화 시점은 사용자 판단에 맡겼다.

## Work Log: Project Workflow Rules

## Result

Complete.

`AGENTS.md` now carries the project overview, primary design goals, reference documents, core principles, response tone, requirement handling procedure, documentation rules, analysis and knowledge-base maintenance rules, original-asset handling rules, implementation rules, task unit rules, Git workflow rules, architecture rules, development philosophy, and legal scope.

Added `docs/PROJECT_CHARTER.md`, `docs/CODING_STYLE.md`, both `EXE_DESIGN` documents, and the `analysis`, `kb`, and `guides` indexes.

Added `LICENSE` (BSD 3-Clause) and `THIRD_PARTY_NOTICES.md`. rePIU states a license policy but has no `LICENSE` file, leaving its redistribution terms unclear; the file was added here from the start rather than reproducing that state. `VERSION` starts at `0.0.1`.

## Adjustments from rePIU

Platform directories became `src/platform/{windows,linux,web}/`, since a 64-bit Windows host makes the name `win32` misleading. The environment boundary moved from DOS/DPMI interrupts to the Win32 import thunk. Wine joined the excluded prior art, with its LGPL license conflict recorded as the reason. Original-asset handling became its own section, because the HDD directory input is this project's entire asset path. A rule barring host OS headers from the shared core was added, as was the principle that unverified statements are not written as fact.

## Verification

Confirmed document creation with `find . -name '*.md'`. Confirmed that the `src/platform/{windows,linux,web}/` directories named by the implementation rules exist and each carries a `README.md` stating its purpose. Confirmed that the license policy, the `LICENSE` file, and `THIRD_PARTY_NOTICES.md` agree.

The repository is not a Git repository yet, so no branch check was performed; when to initialise it was left to the user.
