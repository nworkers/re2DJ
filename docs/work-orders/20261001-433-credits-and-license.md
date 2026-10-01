# 작업 지시 433: CREDITS 문서와 사이트 크레딧 페이지

설계: [20261001-433-credits-and-license.md](../design/20261001-433-credits-and-license.md)

## 작업 항목

1. `CREDITS.md` — 저장소 루트, 한국어/영어. 원작(EZ2DJ·EZ2Dancer), 참조한 프로젝트(MAME — CHD 포맷·보존), 실행 파일에 포함되는 오픈소스(SDL, SDL_mixer, Dear ImGui, spdlog, libchdr, GNU Unifont), 개발·분석 도구(Capstone), 사이트와 문서(Galmuri, Jinja, markdown-it-py, Mermaid), 도구(Claude).
2. `README.md` 라이선스 절 — LICENSE·THIRD_PARTY_NOTICES.md·CREDITS.md 세 문서의 역할을 한/영 모두에 기술.
3. 사이트 크레딧 페이지
   * `docs/sites/site.toml` — `[[credits]]` 항목(설계 표의 key/group/name/version/license/url)
   * `docs/sites/i18n/ko.toml`·`en.toml` — `nav.credits`, `[credits]` 섹션, `[credits.items]` 설명
   * `docs/sites/templates/credits.html` 신설, `base.html` 메뉴에 F4 크레딧(GitHub은 F5)
   * `scripts/site/build_site.py` — `credits` 페이지와 `CREDIT_GROUPS` 컨텍스트
   * `docs/sites/static/css/site.css` — `.credits` 그리드와 모바일 1열
   * `docs/sites/README.md` — 크레딧 페이지 갱신 방법 추가

## 검증

* WSL에서 `python3 scripts/site/build_site.py --offline` 성공, 내부 링크 검사 통과
* headless Edge로 한국어/영어 크레딧 페이지 캡처 확인

## 완료 조건

* 위 파일이 모두 커밋되고 검증이 통과한다.
* 작업 로그를 남긴다.

---

# Work order 433: CREDITS document and a site credits page

Design: [20261001-433-credits-and-license.md](../design/20261001-433-credits-and-license.md)

## Items

1. Root bilingual `CREDITS.md`: the original games (EZ2DJ, EZ2Dancer), referenced projects (MAME — CHD format and preservation), open source in the executables (SDL, SDL_mixer, Dear ImGui, spdlog, libchdr, GNU Unifont), development/analysis tools (Capstone), site and documentation (Galmuri, Jinja, markdown-it-py, Mermaid), tools (Claude).
2. README licence section describing the roles of LICENSE, THIRD_PARTY_NOTICES.md and CREDITS.md in both languages.
3. Site credits page: `[[credits]]` entries in `site.toml`; `nav.credits`, `[credits]` and `[credits.items]` in both i18n files; a new `credits.html` template with an F4 menu entry (GitHub moves to F5); the `credits` page and `CREDIT_GROUPS` context in `build_site.py`; the `.credits` grid CSS with the mobile fallback; and an update to `docs/sites/README.md`.

## Verification

Offline WSL build with the link check, plus headless-Edge captures of the Korean and English credits pages.

## Done when

All files above are committed, verification passes, and a work log is written.
