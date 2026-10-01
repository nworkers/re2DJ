# 작업 로그 433: CREDITS 문서와 사이트 크레딧 페이지

* 작업 지시: [20261001-433-credits-and-license.md](../work-orders/20261001-433-credits-and-license.md)
* 설계: [20261001-433-credits-and-license.md](../design/20261001-433-credits-and-license.md)

## 한 일

* rePIU Task 765의 문서 역할 분리를 re2DJ에 적용했다. re2DJ에는 BSD 3-Clause `LICENSE`가 이미 있어 새로 만들지 않았고, `THIRD_PARTY_NOTICES.md`는 법적 서드파티 목록 역할을 그대로 유지한다.
* 저장소 루트에 한국어/영어 `CREDITS.md`를 추가했다.
  * 원작: EZ2DJ(1999, Amuse World 최초 발매)와 EZ2Dancer. 현재 권리 귀속은 단정하지 않고 "모든 상표와 저작물의 권리는 각 권리자에게"로 일반화했다.
  * 참조한 프로젝트: MAME — CHD 포맷과 보존 작업 감사. 코드 이식은 없음을 명시(코드 수준 크레딧은 libchdr).
  * 실행 파일에 포함되는 오픈소스: SDL, SDL_mixer, Dear ImGui, spdlog, libchdr(LZMA·miniz·zstd·dr_flac 내장), GNU Unifont(ASCII 글리프).
  * 개발·분석 도구: Capstone(game-state-hunt 스크립트, 저장소 미포함). 단위 테스트는 자체 harness라 서드파티 테스트 프레임워크 크레딧이 없다.
  * 사이트와 문서: Galmuri, Jinja, markdown-it-py, Mermaid. 도구: Claude.
* `README.md` 라이선스 절이 LICENSE·THIRD_PARTY_NOTICES.md·CREDITS.md 세 문서의 역할을 한/영 모두에서 가리키게 했다.
* 사이트에 F4 크레딧 페이지를 추가했다(GitHub은 F5로 이동).
  * `site.toml` `[[credits]]` 11개 항목(runtime 6, dev 1, site 4). 버전은 `THIRD_PARTY_NOTICES.md`, `third_party/unifont/README.md`, `scripts/site/requirements.txt`에서 확인된 값만 썼고, libchdr(스냅샷)와 Capstone(미포함 스크립트 의존성)은 버전을 비웠다.
  * `i18n/ko.toml`·`en.toml`에 `nav.credits`, `[credits]`, `[credits.items]`를 같은 키 구조로 추가.
  * `templates/credits.html` 신설: 원작 박스 → 그룹별 목록(라이선스 배지) → runtime 그룹 아래 MAME 감사 각주 → 전체 문서 링크.
  * `build_site.py`: `PAGES`에 `credits`, `CREDIT_GROUPS = ("runtime", "dev", "site")`, 그룹 컨텍스트.
  * CSS `.credits` 그리드(13ch 배지 + 내용)와 600px 이하 1열 전환.
  * `docs/sites/README.md`에 크레딧 페이지 갱신 방법을 추가했다.

## 검증

* WSL에서 `python3 scripts/site/build_site.py --offline` 성공, 내부 링크 검사 통과(`internal links: ok`). 크레딧 페이지가 한국어 `/credits.html`, 영어 `/en/credits.html`로 생성됐다.
* i18n 두 파일의 `[credits]` 키 구조 일치는 Jinja2 StrictUndefined 빌드 통과로 확인된다.
* headless Edge 캡처로 한국어/영어 크레딧 페이지의 레이아웃(배지 그리드, 그룹 박스, 전체 문서 링크, F4 활성 메뉴)을 눈으로 확인했다.

## English

# Work log 433: CREDITS document and a site credits page

Done: applied rePIU Task 765's document-role split to re2DJ. re2DJ already had a BSD 3-Clause `LICENSE`, so none was created; `THIRD_PARTY_NOTICES.md` remains the legal third-party inventory. Added a bilingual root `CREDITS.md` — the original games (EZ2DJ, first released by Amuse World in 1999, and EZ2Dancer, with current ownership generalised to the respective rights holders), the MAME acknowledgement for the CHD format and preservation with an explicit note that no MAME code is adapted, the open source in the executables (SDL, SDL_mixer, Dear ImGui, spdlog, libchdr with its bundled codecs, GNU Unifont ASCII glyphs), the analysis tool Capstone (script-only, not in the repository; the unit tests use an in-house harness), the site stack (Galmuri, Jinja, markdown-it-py, Mermaid) and Claude. The README licence section now points at all three documents in both languages. The site gained an F4 credits page (GitHub moved to F5): eleven `[[credits]]` entries in `site.toml` with versions taken only from verified sources (libchdr and Capstone left unversioned), matching `[credits]` strings in both i18n files, a new `credits.html` template with licence badges, the MAME note under the runtime group and a full-documents box, the `credits` page and group order in `build_site.py`, the `.credits` grid CSS with the mobile fallback, and a maintenance note in `docs/sites/README.md`.

Verification: the offline WSL build succeeded with the internal link check passing and both language pages generated; the identical i18n key structure is enforced by the StrictUndefined build; headless-Edge captures of the Korean and English credits pages were inspected (badge grid, group boxes, full-document links, active F4 menu entry).
