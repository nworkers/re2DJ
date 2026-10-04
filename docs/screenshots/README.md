# 스크린샷 / Screenshots

README와 프로젝트 사이트의 소개에 쓰는 화면입니다. 원본 실행 파일이 re2DJ 위에서 그린 화면을 캡처했으며, 원본 HDD·실행 파일·게임 데이터는 저장소에 포함하지 않습니다.

*Screens used in the README and on the project site's introduction, captured from what the original executables draw on re2DJ. The original HDDs, executables and game data are not part of the repository.*

| 파일 | 타깃 | 장면 |
| --- | --- | --- |
| `ez2dj1stse-title.jpg` | `ez2dj1stse` | 타이틀 / title |
| `ez2dj4th-title.jpg` | `ez2dj4th` | 타이틀 / title |
| `ez2dj4th-demo-play.jpg` | `ez2dj4th` | 데모 플레이 / demo play |
| `ez2dj5th-title.jpg` | `ez2dj5th` | 타이틀 / title |
| `ez2dj6th-title.jpg` | `ez2dj6th` | 타이틀 / title |
| `ez2d2m-title.jpg` | `ez2d2m` | 타이틀 / title |

## 캡처 방법 / How they were taken

- v0.0.63(작업 453)의 Windows x86 Release 빌드로 `re2dj <target>`을 기본 창(2배, 1280x960)으로 실행했습니다.
- 입력 없이 어트랙트 화면을 60초 동안 3초마다 창의 클라이언트 영역에서 캡처하고, 그중 한 장을 골랐습니다.
- 640x480으로 줄여 JPEG(품질 90)로 저장했습니다.
- 사이트 빌드(`scripts/site/build_site.py`)는 이 디렉터리를 산출물의 `screenshots/`로 복사합니다. 사이트에 보일 목록과 순서는 `docs/sites/site.toml`의 `[[screenshots]]`, 설명은 `docs/sites/i18n/*.toml`의 `[screenshots.captions]`에 있습니다.

*Taken with the Windows x86 Release build of v0.0.63 (task 453), running `re2dj <target>` in the default 2x window (1280x960); the window's client area was captured every 3 s for 60 s of attract screens with no input, one frame chosen, scaled to 640x480 and saved as JPEG (quality 90). The site build (`scripts/site/build_site.py`) copies this directory to `screenshots/` in its output; the list and order shown on the site are in `[[screenshots]]` of `docs/sites/site.toml`, and the captions in `[screenshots.captions]` of `docs/sites/i18n/*.toml`.*
