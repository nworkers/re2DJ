# 작업 지시 347: 플랫폼 비트 폭 디렉터리 규칙 / Work order 347: platform bit-width directory policy

설계: [플랫폼 비트 폭 디렉터리 규칙](../design/20260921-347-platform-architecture-directories.md)

*Design: [platform bit-width directory policy](../design/20260921-347-platform-architecture-directories.md)*

## 구현 순서 / Implementation sequence

1. `AGENTS.md` 구현 규칙에 OS 루트와 `x86/`·`x64/` 책임을 한국어/영어로 추가합니다.
   *Add the OS-root and `x86/`/`x64/` responsibilities to the Korean and English implementation rules in `AGENTS.md`.*
2. `docs/CODING_STYLE.md`의 기존 Windows/Linux 디렉터리 정책을 새 기준으로 교체합니다.
   *Replace the existing Windows/Linux directory policy in `docs/CODING_STYLE.md` with the new rule.*
3. `ARCHITECTURE.md`와 플랫폼 README에 source/header 배치 계약을 반영합니다.
   *Reflect the source/header placement contract in `ARCHITECTURE.md` and the platform READMEs.*
4. 현재 구조에서 향후 이동 대상이 되는 대표 범위를 기록하고 문서 일관성을 검증합니다.
   *Record representative existing areas that need later movement and verify documentation consistency.*

## 완료 조건 / Completion criteria

- `windows/`, `linux/` 루트에는 OS 전용이면서 비트 폭 중립 또는 공용인 코드만 둔다는 규칙이 명시됩니다.
- 32비트 전용은 `x86/`, 64비트 전용은 `x64/`라는 이름이 source와 공개 구현 header에 동일하게 적용됩니다.
- 게스트 PE32 의미와 host 비트 폭 종속성을 구분하는 판정 기준이 있습니다.
- 기존 대량 파일 이동이 이번 문서 작업 범위 밖이고 후속 구조 작업임을 명시합니다.

*Completion requires an explicit platform-root rule for width-neutral/shared code, matching `x86/` and `x64/` names for 32-bit-only and 64-bit-only source/public implementation headers, a criterion separating guest PE32 semantics from host-width dependence, and a clear statement that bulk movement of existing files is a later structural task.*
