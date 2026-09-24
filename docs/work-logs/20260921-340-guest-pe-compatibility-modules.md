# 작업 로그 340: 게스트 PE 호환 모듈 계획 / Work log 340: Guest PE compatibility module plan

## 결과 / Result

`kernel32`, `user32` 등을 DLL별 독립 호환 모듈로 구성하는 설계와 후속 구현 계획을 작성했습니다. 공용 descriptor 목록을 registry lookup, PE32 export table, static IAT와 dynamic `GetProcAddress`의 단일 source of truth로 삼습니다. Linux는 실제 guest-visible PE32 facade를 mapping하고, Windows는 system DLL shadowing 없이 같은 descriptor를 기존 WoW64 injected-runtime adapter에서 사용하도록 플랫폼 차이를 명시했습니다.

*Documented the design and follow-up implementation plan for independent compatibility modules such as `kernel32` and `user32`. A shared descriptor list is the single source of truth for registry lookup, PE32 export tables, static IAT binding, and dynamic `GetProcAddress`. The platform distinction is explicit: Linux maps real guest-visible PE32 facades, while Windows reuses the descriptors through the existing WoW64 injected-runtime adapter without shadowing system DLLs.*

첫 구현은 공용 registry, PE32 facade builder, 확인된 네 `kernel32` export, resolver 이관의 네 작업으로 분할했습니다. guest handle/VFS 의미, 다른 DLL, Linux x64 trampoline과 Windows adapter는 각각 후속 설계로 분리했습니다. 코드나 생성 DLL binary는 이번 작업에서 추가하지 않았습니다.

*Split the first implementation into four tasks: shared registry, PE32 facade builder, four confirmed `kernel32` exports, and resolver migration. Guest-handle/VFS semantics, other DLLs, the Linux x64 trampoline, and the Windows adapter remain separate follow-up designs. This task adds no code or generated DLL binary.*

## 검토 / Review

- 프로젝트 charter, 현재 architecture, Win32 HLE porting plan, 실행 파일 설계, 코딩 스타일을 검토했습니다.
- Linux single-process backend, dynamic resolver, CreateFileA thunk 설계와 실제 첫 import 분석을 검토했습니다.
- Wine 7.0/8.0 공개 release note에서 PE-facing module과 Unix-side service 분리 개념만 확인했으며 Wine 코드 재사용은 계획에서 제외했습니다.
- 문서 경로와 링크, 한영 병기, `git diff --check`를 검증 대상으로 삼았습니다.

*Review performed:*

- *Reviewed the project charter, current architecture, Win32 HLE porting plan, executable design, and coding style.*
- *Reviewed the Linux single-process backend, dynamic resolver, CreateFileA-thunk designs, and real first-import analysis.*
- *Used the public Wine 7.0/8.0 release notes only to confirm the conceptual split between PE-facing modules and Unix-side services; Wine code reuse is excluded.*
- *Selected document paths and links, Korean/English pairing, and `git diff --check` for validation.*

## 검증 / Validation

- 새 문서의 저장소 내부 Markdown 링크가 모두 존재하는지 확인했습니다.
- 변경 문서의 trailing whitespace 검사를 통과했습니다.
- 코드와 build 구성을 변경하지 않은 계획 전용 작업이므로 제품 build와 단위 테스트는 실행하지 않았습니다.

*Validation performed:*

- *Confirmed that every repository-local Markdown link in the new documents exists.*
- *Passed the trailing-whitespace check for all changed documents.*
- *Did not run product builds or unit tests because this planning-only task changes no code or build configuration.*
