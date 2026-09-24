# 작업 346: 런타임 산출물 ignore 정책 / Task 346: runtime artifact ignore policy

## 목표 / Objective

저장소 루트의 서로 독립된 `roms/`와 `overlays/` 디렉터리는 각각 디렉터리 자리표시자 `dir.txt`만 추적하고, 원본 자산·게스트 overlay·실행 로그가 실수로 Git에 추가되지 않도록 ignore 계약을 명시합니다.

*In the separate repository-root `roms/` and `overlays/` directories, track only a `dir.txt` directory placeholder in each, and make the ignore contract explicit so original assets, guest overlays, and runtime logs cannot be added to Git accidentally.*

## 정책 / Policy

- `/roms/*`와 `/overlays/*`는 기본적으로 모두 ignore하고, 각 디렉터리의 `dir.txt`만 예외로 추적합니다.
  *Ignore `/roms/*` and `/overlays/*` by default, with only each directory's `dir.txt` exempted for tracking.*
- 두 `dir.txt`는 모두 0바이트를 유지합니다. 실제 ROM과 overlay 데이터는 모두 ignore합니다.
  *Keep both `dir.txt` files at zero bytes. Ignore all actual ROM and overlay data.*
- `/logs/`는 내용 전체를 ignore하며 새 로그를 커밋하지 않습니다. 정책 적용 시 이미 추적된 `/logs/` 파일이 있으면 인덱스와 저장소에서 삭제하되 로컬의 ignore된 분석 로그는 불필요하게 삭제하지 않습니다.
  *Ignore all contents of `/logs/` and never commit new logs. When applying the policy, remove any already-tracked `/logs/` files from the index and repository, without unnecessarily deleting local ignored analysis logs.*

## 검증 / Validation

`git check-ignore`로 일반 ROM, overlay 데이터와 로그가 ignore되는지 확인하고, `roms/dir.txt`와 `overlays/dir.txt`만 ignore되지 않는지 확인합니다. `git ls-files`로 두 디렉터리의 추적 집합이 각 `dir.txt`뿐이며 `/logs/`에는 추적 파일이 없는지 확인합니다.

*Use `git check-ignore` to verify that ordinary ROM files, overlay data, and logs are ignored while `roms/dir.txt` and `overlays/dir.txt` are not. Use `git ls-files` to verify that the tracked set in the two directories contains only their `dir.txt` files and no `/logs/` files.*
