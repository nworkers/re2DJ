# 작업 지시 432: re2DJ GitHub Pages 프로젝트 사이트

설계: [20261001-432-github-pages-site.md](../design/20261001-432-github-pages-site.md)

## 작업 항목

1. `docs/sites/` — 사이트 소스
   * `site.toml` — 저장소, 기본 URL, 언어, 콘텐츠 경로, Mermaid 주소, 플랫폼 상태
   * `i18n/ko.toml`, `i18n/en.toml` — 페이지 문구(두 파일의 키 구조 동일)
   * `templates/` — `base`, `index`, `wip`, `download`, `post`, `404` Jinja2 템플릿
   * `static/` — EZ2DJ 초기 버전 분위기의 CSS, 언어 전환 JS, favicon, Galmuri 폰트(rePIU에서 복사, SIL OFL 1.1)
   * `README.md` — 디렉터리 안내(한국어/영어)
2. `docs/post/README.md` — 개발 기록 작성 지침(파일명 규칙, `## English` 분리 규칙)
3. `scripts/site/` — `build_site.py`, `content.py`, `releases.py`, `requirements.txt`
   * 언어 분리: `## English` 행 우선, `---` 규칙 fallback
   * 타깃 카탈로그: `src/target/target_profile.cpp`의 두 선언 형태 파싱 + 입력 형식(HDD/CHD) 추출
   * 릴리스 자산 분류: `re2dj-v*-windows-x86.zip`=package, `*.sha256`=checksum
4. `.github/workflows/pages.yml` — 빌드·배포 워크플로(PR은 빌드와 링크 검사만)

## 검증

* WSL에서 `python3 scripts/site/build_site.py --offline` 성공, 내부 링크 검사 통과
* 빌드 출력에서 내장 프로파일 8개와 릴리스 노트 언어 분리 확인
* 산출물 페이지를 열어 한국어/영어와 디자인 확인

## 완료 조건

* 위 파일이 모두 커밋되고 검증이 통과한다.
* 작업 로그를 남긴다.
* 배포는 main 머지와 저장소 Settings → Pages → Source를 GitHub Actions로 설정한 뒤 이루어진다(사용자 수행).

---

# Work order 432: re2DJ GitHub Pages project site

Design: [20261001-432-github-pages-site.md](../design/20261001-432-github-pages-site.md)

## Items

1. `docs/sites/` — site source: `site.toml`; `i18n/ko.toml` and `i18n/en.toml` with identical key structure; the `base`, `index`, `wip`, `download`, `post`, `404` Jinja2 templates; static CSS in the early-EZ2DJ mood, the language-switch JS, a favicon and the Galmuri fonts copied from rePIU (SIL OFL 1.1); a bilingual `README.md`.
2. `docs/post/README.md` — dev-log authoring guidelines (file naming, `## English` split rule).
3. `scripts/site/` — `build_site.py`, `content.py`, `releases.py`, `requirements.txt`, with the `## English`-first language splitter, the two-form catalog parser with HDD/CHD input kinds, and the re2DJ asset classification (package zip, sha256 checksum).
4. `.github/workflows/pages.yml` — build and deploy workflow; pull requests build and link-check only.

## Verification

* `python3 scripts/site/build_site.py --offline` under WSL succeeds and the internal link check passes.
* Build output confirms the eight built-in profiles and the bilingual release-note split.
* Output pages are opened to inspect both languages and the design.

## Done when

* All files above are committed and verification passes.
* A work log is written.
* Deployment happens after merging to main and setting Settings → Pages → Source to GitHub Actions (performed by the user).
