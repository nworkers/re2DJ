# 작업 지시 346: 런타임 산출물 ignore 정책 / Work order 346: runtime artifact ignore policy

설계: [런타임 산출물 ignore 정책](../design/20260921-346-runtime-artifact-ignore-policy.md)

*Design: [runtime artifact ignore policy](../design/20260921-346-runtime-artifact-ignore-policy.md)*

## 구현 순서 / Implementation sequence

1. `AGENTS.md`의 Git 규칙에 서로 독립된 `roms/dir.txt`·`overlays/dir.txt` placeholder와 `/logs/` 전체 ignore 정책을 추가합니다.
   *Add the separate `roms/dir.txt` and `overlays/dir.txt` placeholders plus the complete `/logs/` ignore policy to the Git rules in `AGENTS.md`.*
2. `.gitignore`에서 `/roms/`와 `/overlays/` 각각 `dir.txt`만 추적 가능하게 하고 `/logs/` 전체 ignore를 유지합니다.
   *Make only each `dir.txt` trackable under `/roms/` and `/overlays/` in `.gitignore`, while retaining complete `/logs/` ignore.*
3. 두 디렉터리에 0바이트 `dir.txt`를 추가하고 이미 추적된 ROM/overlay/log 파일을 제거합니다.
   *Add a zero-byte `dir.txt` to both directories and remove any already-tracked ROM, overlay, or log files.*
4. ignore 및 tracked-file 집합을 검증하고 작업 로그를 남깁니다.
   *Verify the ignore and tracked-file sets and leave a work log.*

## 완료 조건 / Completion criteria

- `roms/dir.txt`와 `overlays/dir.txt`의 크기가 모두 0이고 이 파일들만 각 디렉터리에서 추적됩니다.
- `/roms/`와 `/overlays/`의 다른 파일 및 `/logs/`의 모든 파일이 ignore됩니다.
- 저장소에 추적된 `/logs/` 파일이 없습니다.

*Completion requires zero-byte tracked `roms/dir.txt` and `overlays/dir.txt` files as the only tracked files in their directories, all other `/roms/` and `/overlays/` files plus every `/logs/` file ignored, and no tracked `/logs/` files in the repository.*
