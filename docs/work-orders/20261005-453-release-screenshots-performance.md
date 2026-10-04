# 작업 453 작업 지시서 — 릴리스 스크린샷과 성능 측정 기록 / Task 453 work order — release screenshots and the performance record

설계: [20261005-453-release-screenshots-performance.md](../design/20261005-453-release-screenshots-performance.md)

## 절차 / Steps

1. 성능 측정 결과를 `docs/analysis/windows-in-process-performance.md`로 쓰고 분석 색인에 더한다. 후속 분석을 `docs/TODO.md`에 등록한다.
   *Write the measurements as `docs/analysis/windows-in-process-performance.md`, add it to the analysis index, and register the follow-up analysis in `docs/TODO.md`.*
2. Windows x86 Release 빌드로 다섯 타깃을 실행해 캡처하고, 고른 화면을 640x480 JPEG로 `docs/screenshots/`에 둔다. 디렉터리 README에 목록과 캡처 방법을 적는다.
   *Capture the five targets with the Windows x86 Release build and put the chosen frames in `docs/screenshots/` as 640x480 JPEG, with a README listing them and how they were taken.*
3. README에 스크린샷 절을 더하고, 오래된 경고·Mermaid 표기를 고친다.
   *Add a screenshots section to the README and fix the stale warning and Mermaid labels.*
4. 사이트: `site.toml` `[[screenshots]]`, `i18n` `[screenshots]`, `index.html` 격자, `site.css`, `build_site.py`의 복사와 context, 사이트 README, "동작 방식" 문구.
   *Site: `[[screenshots]]` in `site.toml`, `[screenshots]` in the i18n files, the grid in `index.html`, `site.css`, the copy and context in `build_site.py`, the site README, and the "How it works" copy.*
5. v0.0.63 릴리스 노트(`RELEASE_NOTES.md`, `docs/release-notes/v0.0.63.md`), `VERSION`, `docs/IMPLEMENTED.md`.
   *The v0.0.63 release notes (`RELEASE_NOTES.md`, `docs/release-notes/v0.0.63.md`), `VERSION`, and `docs/IMPLEMENTED.md`.*
6. 검증 후 브랜치 전체를 main에 squash 머지하고 `v0.0.63` 태그를 단다.
   *After verification, squash the whole branch into main and tag `v0.0.63`.*

## 완료 조건 / Done when

사이트 빌드와 링크 검사가 통과하고 렌더링이 확인되며, Windows Release 빌드가 성공하고, main에 v0.0.63 커밋과 태그가 있다.

*The site build and link check pass with the render checked, the Windows Release build succeeds, and main carries the v0.0.63 commit and tag.*
