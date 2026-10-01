# 작업 로그 432: re2DJ GitHub Pages 프로젝트 사이트

* 작업 지시: [20261001-432-github-pages-site.md](../work-orders/20261001-432-github-pages-site.md)
* 설계: [20261001-432-github-pages-site.md](../design/20261001-432-github-pages-site.md)

## 한 일

* rePIU 프로젝트 사이트(rePIU Task 756)의 작업 방식을 그대로 가져와 re2DJ용 사이트 소스를 만들었다.
  * `docs/sites/` — `site.toml`, `i18n/ko.toml`·`i18n/en.toml`, Jinja2 템플릿 6종, CSS/JS/favicon, Galmuri 폰트(woff2 3종 + OFL.txt, rePIU에서 복사), 디렉터리 README.
  * `scripts/site/` — `build_site.py`, `content.py`, `releases.py`, `requirements.txt`.
  * `.github/workflows/pages.yml` — main 관련 경로 push, Release 워크플로 성공, 수동 실행에서 빌드·배포. PR은 빌드와 링크 검사만.
  * `docs/post/README.md` — 개발 기록 작성 지침.
  * `docs/release-notes/README.md` — 노트가 사이트에도 게시된다는 안내를 추가.
* re2DJ에 맞춘 변경점:
  * 언어 분리는 이 저장소 릴리스 노트 관례인 `## English` 행을 우선 지원하고, rePIU의 `---` 규칙을 fallback으로 유지했다.
  * 타깃 카탈로그 파서는 `entry.profile.id/display_name` 직접 대입과 `MakeChdCompatibilityProfile(...)` 호출 두 형태를 소스 순서대로 읽고, 블록의 `HddInputKind::kMameChd`로 HDD/CHD 입력 형식을 구분한다.
  * 릴리스 자산 분류: `*-windows-x86.zip`=package, `*.sha256`=checksum.
  * 릴리스 요약은 heading보다 앞에 있는 선두 문단만 쓴다. 이 저장소 노트는 heading+목록 구조여서, 종전 규칙(첫 문단)대로면 모든 릴리스 요약이 말미의 자산 미포함 고지 문장으로 잡혔다.
* 디자인: Galmuri 픽셀 폰트 기반 8비트 레트로 위에 EZ2DJ 초기 버전(1st/1st SE) 분위기를 입혔다.
  * 검정에 가까운 남색 바탕, 네온 시안 제목(옅은 glow), 호박색 링크, 빨강·초록 포인트.
  * 메뉴 바 아래에 1인 플레이 기어 색 띠(빨강 턴테이블, 흰/파랑 건반, 초록 페달), `body::after` 정적 CRT 스캔라인, 히어로 타이틀 `re2DJ`의 `2`만 빨강, 404의 "INSERT COIN TO CONTINUE".
  * favicon은 기어를 픽셀로 그린 SVG.

## 검증

* WSL(Python 3.12.3)에서 `python3 scripts/site/build_site.py --offline` 성공.
  * 타깃 8개를 정확히 읽음: ez2dj1st·ez2dj2nd(hdd), ez2dj1stse·ez2dj3rd·ez2dj4th·ez2dj5th·ez2dj6th·ez2d2m(chd).
  * 릴리스 노트 5건 모두 파일에서 읽고 내부 링크 검사 통과(`internal links: ok`).
  * 언어 분리 확인: 한국어 `wip.html`에 "변경 사항" 5회, 영어 `en/wip.html`에 "Changes" 5회·"변경 사항" 0회.
* Edge headless 스크린샷으로 index(ko)·download(ko)·wip(ko)·index(en, 좁은 폭)·404를 눈으로 확인.
  * 발견·수정 1: index ASCII 도식에 공용 `pre`의 `overflow-x:auto` 때문에 불필요한 스크롤바가 생겨 `.diagram`을 `overflow: visible`로 바꿨다.
  * 발견·수정 2: 릴리스 요약이 매 릴리스 동일한 고지 문장으로 잡혀 선두 문단 규칙으로 바꿨다(위 변경점).
  * 404는 설계대로 절대 URL로 정적 자산을 참조하므로 로컬 파일 열기에서는 스타일이 빠져 보인다. 배포 주소에서는 적용된다.
  * 좁은 폭 캡처의 우측 잘림은 Edge headless 최소 창 폭 클램프로 인한 캡처 아티팩트로 판단했다(미디어 쿼리 적용은 F키 힌트 숨김으로 확인).
* `build/`는 기존 `.gitignore`로 무시되어 산출물은 커밋되지 않는다.

## 남은 일 (사용자 수행)

* main 머지 후 저장소 Settings → Pages → Source를 **GitHub Actions**로 설정해야 첫 배포가 이루어진다.
* VSCode의 workflow lint가 `environment: github-pages`를 미생성 environment라고 경고하지만, Pages를 GitHub Actions 소스로 켜면 자동 생성된다(rePIU와 동일 구성).

## English

# Work log 432: re2DJ GitHub Pages project site

Done: ported the rePIU project-site working method (rePIU Task 756) to re2DJ — `docs/sites/` (config, i18n, six Jinja2 templates, CSS/JS/favicon, Galmuri fonts copied from rePIU with OFL.txt, README), `scripts/site/` (build scripts and pinned requirements), `.github/workflows/pages.yml`, `docs/post/README.md`, and a note in `docs/release-notes/README.md` that notes are also published on the site. re2DJ-specific changes: the language splitter prefers this repository's `## English` separator with the `---` rule as fallback; the catalog parser reads both declaration shapes of `src/target/target_profile.cpp` in source order and derives the HDD/CHD input kind; release assets classify as `*-windows-x86.zip` = package and `*.sha256` = checksum; a release summary uses only a lead paragraph that precedes every heading (this repository's notes are heading+list shaped, so the old first-paragraph rule picked the closing no-assets notice for every release). Design: an 8-bit retro base with the early-EZ2DJ mood — near-black navy, neon cyan headings, amber links, the single-player gear strip under the menu bar, a static CRT scanline overlay, the red `2` in the hero title and an "INSERT COIN TO CONTINUE" 404, plus a pixel-gear SVG favicon.

Verification: `python3 scripts/site/build_site.py --offline` under WSL (Python 3.12.3) succeeded; all eight targets were read with correct input kinds; all five release-note files loaded; the internal link check passed; the bilingual split was confirmed (five "변경 사항" sections in Korean `wip.html`, five "Changes" and zero Korean headings in `en/wip.html`). Headless-Edge screenshots of index/download/wip (ko), narrow-width index (en) and 404 were inspected; two issues found and fixed during review (a stray scrollbar on the ASCII diagram from the shared `pre` overflow rule, and the duplicated release summary described above). The 404 page references static assets by absolute URL by design, so it is unstyled when opened from a local file. `build/` stays ignored by the existing `.gitignore`.

Remaining (user): after merging to main, set Settings → Pages → Source to **GitHub Actions** for the first deployment. The VSCode workflow lint warning about the `github-pages` environment disappears once Pages is enabled, as in rePIU.
