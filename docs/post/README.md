# 개발 기록 작성 지침

이 디렉터리의 글은 프로젝트 사이트(<https://nworkers.github.io/re2DJ/>)의 "개발 기록"으로 게시됩니다. main에 머지되면 Pages 워크플로가 자동으로 반영합니다.

## 파일명

```text
YYYY-MM-DD-NNNNNN-slug.md
```

* `YYYY-MM-DD` — 게시 날짜
* `NNNNNN` — 관련 작업 번호 6자리 (예: 작업 432 → `000432`)
* `slug` — 소문자와 하이픈으로 된 짧은 제목

예: `2026-10-01-000432-github-pages-site.md`

이 형식이 아닌 파일(이 README 포함)은 게시되지 않습니다.

## 본문 구조

릴리스 노트와 같은 규칙입니다. 한국어 전문을 먼저 쓰고, `## English` 행 아래에 영어 전문을 씁니다.

```markdown
# 글 제목

본문...

## English

# Post title

Body...
```

* 각 언어의 첫 heading이 그 언어 페이지의 제목이 됩니다.
* 첫 문단이 목록 페이지의 요약으로 쓰입니다.
* 저장소 상대 링크는 GitHub으로, 이 디렉터리의 다른 글 링크는 사이트 글 페이지로 자동 변환됩니다.
* Mermaid 코드 블록은 게시 페이지에서 도식으로 렌더링됩니다.
* 원본 게임 자산(바이트 덤프, 화면 캡처 포함)은 싣지 않습니다.

## English

# Dev-log authoring guidelines

Posts in this directory are published as the "Dev log" of the project site (<https://nworkers.github.io/re2DJ/>). The Pages workflow picks them up when they reach main.

## File name

`YYYY-MM-DD-NNNNNN-slug.md` — the publish date, the related six-digit task number (task 432 → `000432`) and a short lowercase hyphenated slug, e.g. `2026-10-01-000432-github-pages-site.md`. Files not matching this pattern (including this README) are not published.

## Body structure

Same rule as the release notes: the full Korean document first, then the full English document under a `## English` line. Each language's first heading becomes that page's title, and the first paragraph becomes the list-page summary. Repository-relative links are rewritten to GitHub, links to other posts in this directory become site links, and Mermaid code blocks render as diagrams. Never include original game assets, byte dumps or captured game screens.
