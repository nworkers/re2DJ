# 작업 로그 346: 런타임 산출물 ignore 정책 / Work log 346: runtime artifact ignore policy

## 결과 / Result

`AGENTS.md`의 Git 규칙과 `.gitignore`를 갱신하여 저장소 루트의 서로 독립된 `roms/`와 `overlays/`에서 각각 0바이트 `dir.txt`만 추적하도록 했습니다. 일반 ROM과 guest overlay 데이터는 모두 ignore합니다. `/logs/` 전체 ignore도 명시적으로 유지했습니다.

*Updated the Git rules in `AGENTS.md` and `.gitignore` so the separate repository-root `roms/` and `overlays/` directories each track only a zero-byte `dir.txt`. All ordinary ROM and guest-overlay data is ignored. The complete `/logs/` ignore remains explicit.*

적용 전에 `git ls-files`와 현재 commit tree를 확인했으며 `/logs/`, `/roms/`, `/overlays/` 아래에는 기존 추적 파일이 없었습니다. 따라서 삭제할 Git 기록은 없었고, 분석을 위해 남아 있는 로컬 ignore 로그는 삭제하지 않았습니다.

*Checked both `git ls-files` and the current commit tree before applying the change; no files were already tracked under `/logs/`, `/roms/`, or `/overlays/`. There was therefore no tracked content to delete, and local ignored analysis logs were preserved.*

## 검증 / Validation

- `roms/dir.txt`, `overlays/dir.txt`: 각 0바이트.
- `git check-ignore -v --no-index`: 두 placeholder는 negation 예외에 일치하고, `roms/sample.payload`, `overlays/sample.payload`, `logs/sample.payload`는 각 루트 ignore 규칙에 일치.
- stage 후 `git ls-files -- logs roms overlays`: 두 `dir.txt`만 표시되고 `/logs/` 파일은 없음.
- `git diff --check`: whitespace 오류 없음.

*Validation confirmed that both placeholders are zero bytes; the two placeholders match the negation exceptions while representative ROM, overlay, and log paths match their root ignore rules; the staged tracked set contains only the two `dir.txt` files and no `/logs/` files; and `git diff --check` reports no whitespace errors.*
