# 작업 456 작업 로그 — 셰이더 비교 스크린샷과 WIP 글 / Task 456 work log — shader comparison screenshots and the WIP post

설계: [20261005-456-shader-screenshots-and-post.md](../design/20261005-456-shader-screenshots-and-post.md) · 지시서: [20261005-456-shader-screenshots-and-post.md](../work-orders/20261005-456-shader-screenshots-and-post.md)

## 2026-10-05

- **지침**: 사용자에게 개발 기록 지침의 캡처 금지 규칙을 물어, 고치고 이미지를 싣기로 했다. `docs/post/README.md` 두 언어를 고쳤다.
- **촬영**: Windows x86 Release로 4th(40초·56초)와 6th(46초)를 셰이더마다 따로 실행해 찍었다. 첫 촬영은 화면 복사(`CopyFromScreen`)라 4th `none`에 다른 창(Pump It Up 화면)이 겹쳐 들어갔다. `PrintWindow`(플래그 3)로 바꿔 세 셰이더 모두 다시 찍었고, 대조표로 9장이 깨끗하며 4th 데모 점수(003166)까지 같은 장면임을 확인했다. 1280x960 JPEG 9장(14~35만 바이트)과 1:1 비교 띠 PNG 3장(10~27만 바이트)을 `docs/screenshots/shaders/`에 두었다.
- **측정**: 4th 어트랙트 25초씩, 셰이더별 FPS·CPU(수치는 [작업 455 로그](20261005-455-post-process-shaders.md)).
- **글·README**: `docs/post/2026-10-05-000455-post-process-shaders-wip.md`(한국어 → `## English` → 영어, Mermaid sequence·xychart), README 셰이더 비교 절(띠 두 장), `docs/screenshots/README.md` 셰이더 절.

  *Rule: the user was asked about the dev-log guideline's capture ban and chose to change it and include images; both languages of `docs/post/README.md` were updated. Capture: Windows x86 Release ran 4th (40 s, 56 s) and 6th (46 s) once per shader. The first pass copied the screen (`CopyFromScreen`), and another window (a Pump It Up picture) overlapped the 4th `none` shots; switching to `PrintWindow` (flags 3), all three shaders were captured again, and a contact sheet showed all nine clean and the same scene down to the 4th demo score (003166). Nine 1280x960 JPEGs (140 to 350 KB) and three 1:1 comparison strips as PNG (100 to 270 KB) went into `docs/screenshots/shaders/`. Measurement: the 4th attract for 25 s per shader (numbers in the [task 455 log](20261005-455-post-process-shaders.md)). Post and README: `docs/post/2026-10-05-000455-post-process-shaders-wip.md` (Korean → `## English` → English, Mermaid sequence and xychart), a shader comparison section in the README (two strips), and a shader section in `docs/screenshots/README.md`.*

- **검증**: 사이트 `build_site.py --offline`이 글을 두 언어로 게시하고(`2026-10-05-post-process-shaders-wip [ko+en]`) `internal links: ok`. 글 속 이미지는 `https://github.com/nworkers/re2DJ/raw/main/docs/screenshots/shaders/…`로 바뀌므로 main에 push된 뒤에 보인다.

  *Verification: the site's `build_site.py --offline` publishes the post in both languages (`2026-10-05-post-process-shaders-wip [ko+en]`) with `internal links: ok`; the post's images become `https://github.com/nworkers/re2DJ/raw/main/docs/screenshots/shaders/…`, so they show once main is pushed.*
