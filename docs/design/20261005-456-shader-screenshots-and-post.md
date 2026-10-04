# 작업 456 설계 — 셰이더 비교 스크린샷과 WIP 글 / Task 456 design — shader comparison screenshots and the WIP post

## 배경 / Background

rePIU는 후처리 셰이더(rePIU 작업 768)를 넣으며 셰이더 비교 스크린샷과 WIP 글을 함께 냈다(rePIU 작업 770). 사용자가 re2DJ도 WIP와 웹사이트를 같은 방식으로 갱신하라고 요청했다(2026-10-05). 소개 페이지 스크린샷 격자는 작업 453에서 이미 같은 구조로 들어가 있으므로, 이번에는 셰이더 비교 화면과 개발 기록 글이 대상이다.

*rePIU shipped shader comparison screenshots and a WIP post with its post-processing shaders (rePIU tasks 768 and 770), and the user asked for the WIP and the site to be updated the same way (2026-10-05). The introduction's screenshot grid already came in with the same structure in task 453, so this task covers the shader comparison screens and the dev-log post.*

## 결정 / Decisions

- **지침 변경**: `docs/post/README.md`는 "화면 캡처 포함" 원본 자산을 싣지 않는다고 했다. 사용자 결정(2026-10-05)으로, 실행 파일·데이터·바이트 덤프는 계속 금지하되 re2DJ가 실행해 그린 화면을 `docs/screenshots/`에 두고 링크하는 것은 허용한다.
- **화면**: 4th 타이틀·데모 플레이, 6th 타이틀을 `none`·`crt`·`scanline`으로 찍는다. 주사선이 출력 픽셀 무늬라 1280x960을 줄이지 않고 JPEG(품질 90)로, 같은 320x240 부분을 1:1로 잘라 나란히 놓은 PNG 띠를 더한다. 위치는 `docs/screenshots/shaders/`.
- **촬영 방법**: 셰이더마다 따로 실행해 같은 시점에 찍는다. 다른 창이 겹쳐도 내용이 섞이지 않도록 `PrintWindow`(PW_CLIENTONLY | PW_RENDERFULLCONTENT)로 창 클라이언트 영역을 받는다.
- **글**: `docs/post/2026-10-05-000455-post-process-shaders-wip.md`. rePIU 글의 구성(결과 화면, 끼운 곳, 고르는 법, 로그, sample test, 알려진 것, 기술 스택)을 따르고, re2DJ가 다른 점(복사 없음, 명령행 선택, 런처 자식 상속)을 적는다. README에는 비교 띠 두 장을 넣는다.
- **사이트**: 글은 Pages 빌드가 개발 기록으로 게시하고 이미지는 GitHub raw 주소로 바뀐다. 사이트 코드 변경은 없다.

*Rule change: `docs/post/README.md` barred original assets "including screen captures"; by the user's decision (2026-10-05) executables, data and byte dumps stay barred while a picture re2DJ drew while running may be linked from `docs/screenshots/`. Screens: 4th title and demo play and 6th title under `none`, `crt` and `scanline`, at full 1280x960 as JPEG (quality 90) because scanlines are an output-pixel pattern, plus a PNG strip of the same 320x240 part cropped 1:1, under `docs/screenshots/shaders/`. Capture: each shader in its own run at the same moments, through `PrintWindow` (PW_CLIENTONLY | PW_RENDERFULLCONTENT) so an overlapping window cannot mix in. Post: `docs/post/2026-10-05-000455-post-process-shaders-wip.md`, following rePIU's layout (results, where it goes in, choosing, log, sample tests, known, technology) and stating where re2DJ differs (no copy, command-line choice, launcher children inheriting); the README gains two comparison strips. Site: the Pages build publishes the post as a dev-log entry with images rewritten to GitHub raw URLs; no site code changes.*
