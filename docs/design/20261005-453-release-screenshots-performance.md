# 작업 453 설계 — 릴리스 스크린샷과 성능 측정 기록 / Task 453 design — release screenshots and the performance record

## 배경 / Background

작업 446~452로 Windows가 주입 경로 대신 Linux와 같은 in-process 러너를 쓰게 되었다. 이 브랜치를 v0.0.63으로 머지하기 전에 사용자가 다음을 요청했다(2026-10-05).

1. 전후 성능 측정 결과를 문서로 남기고, 추가 분석은 TODO로 둔다.
2. 게임 버전별 스크린샷을 `docs` 아래에 저장하고 README와 프로젝트 사이트의 소개에 넣는다.
3. 릴리스 노트를 쓰고 main에 머지한다.

*Tasks 446 to 452 moved Windows from the injection path to the in-process runner Linux uses. Before merging the branch as v0.0.63 the user asked (2026-10-05) to record the before/after performance measurements with further analysis left as TODO, to store per-game screenshots under `docs` and show them in the README and on the project site's introduction, and to write the release notes and merge to main.*

## 결정 / Decisions

- **성능 기록**: 원본 실행으로 얻은 프로젝트 고유 측정이므로 `docs/analysis/windows-in-process-performance.md`에 확인됨·추정·미확정을 나눠 둔다. 후속 분석 항목은 `docs/TODO.md`의 "다음 작업"에 둔다. 측정용 vsync off는 임시 패치로만 썼고 제품 코드에 넣지 않는다. 정식 옵션 여부는 TODO에서 정한다.
- **스크린샷 위치**: `docs/screenshots/`. README는 저장소 상대 경로로, 사이트는 빌드가 이 디렉터리를 산출물 `screenshots/`로 복사해 같은 파일을 쓴다. 사이트의 `static/`에 두지 않는 이유는 README와 사이트가 한 원본을 공유하고, 사이트 전용 자산(CSS·폰트)과 콘텐츠를 나누기 위해서다.
- **스크린샷 범위**: 창을 닫을 때까지 실행되는 다섯 타깃(1st SE, 4th, 5th, 6th, 2nd MOVE)의 타이틀 화면과, 게임 화면을 보여 주는 4th 데모 플레이 한 장. 640x480 JPEG(품질 90), 장당 50~90 KB.
- **사이트**: 목록·순서는 언어 중립이라 `site.toml`의 `[[screenshots]]`(key, file), 설명은 `i18n/*.toml`의 `[screenshots.captions]`. 소개 페이지 hero 바로 아래에 격자로 보이고, 이미지는 원본 크기로 연결한다.
- **함께 고칠 오래된 문구**: README의 "게임은 실행되지 않습니다" 경고와 Mermaid의 "(planned)", 사이트 "동작 방식"의 "Windows에서는 원본 PE32가 main image로 시작" 문장은 스크린샷과 맞지 않으므로 현재 구조로 고친다.

*Performance record: project-specific measurements from original runs, so `docs/analysis/windows-in-process-performance.md` with confirmed, inferred and unresolved kept apart; follow-ups go to "Next work" in `docs/TODO.md`. The vsync-off switch used for measuring stays a temporary patch, not product code; whether it becomes an option is a TODO item. Screenshots live in `docs/screenshots/`: the README links them by repository path and the site build copies the directory to `screenshots/` in its output, so both use one source, kept apart from the site-only assets (CSS, fonts) in `static/`. Scope: the title screens of the five targets that run until their window is closed (1st SE, 4th, 5th, 6th, 2nd MOVE) and one 4th demo-play frame showing gameplay, 640x480 JPEG at quality 90, 50 to 90 KB each. Site: the language-neutral list and order in `[[screenshots]]` (key, file) of `site.toml`, captions in `[screenshots.captions]` of `i18n/*.toml`, shown as a grid right below the hero with each image linking to its full size. Stale copy fixed alongside: the README's "nothing runs yet" warning and "(planned)" in its Mermaid, and the site's "How it works" sentence saying the original PE32 starts as the Windows main image.*

## 검증 / Verification

- 사이트 `build_site.py --offline`이 성공하고 내부 링크 검사가 통과하며, 두 언어의 `index.html`에 이미지 여섯 개가 있다. 헤드리스 브라우저 렌더링을 눈으로 확인한다.
- README의 이미지 경로가 실제 파일을 가리킨다.
- 코드 변경은 사이트 빌드 스크립트뿐이라 제품 빌드는 머지 전 Windows Release 빌드로 확인한다.

*The site's `build_site.py --offline` succeeds with its internal-link check, both languages' `index.html` hold the six images, and a headless-browser render is checked by eye; the README's image paths name real files; the only code change is the site build script, and the product is checked with the Windows Release build before the merge.*
