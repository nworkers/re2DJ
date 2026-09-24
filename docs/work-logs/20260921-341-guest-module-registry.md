# 작업 로그 341: 게스트 모듈 descriptor와 registry / Work log 341: Guest module descriptors and registry

## 결과 / Result

플랫폼 중립 `GuestModuleRegistry`와 export/module/mapping descriptor를 추가했습니다. registry는 명시된 DLL canonical name과 alias를 ASCII 대소문자 무시로 찾고, export name은 대소문자를 구분하며 ordinal은 정확한 16비트 값으로 찾습니다. 등록된 module과 export view는 registry가 값을 소유하므로 이름과 guest base handle lookup에서 process lifetime 동안 안정된 동일 identity를 제공합니다.

*Added a platform-neutral `GuestModuleRegistry` plus export/module/mapping descriptors. The registry finds explicitly declared DLL canonical names and aliases ASCII-case-insensitively, resolves export names case-sensitively and ordinals as exact 16-bit values. Registered module and export views own their values in the registry, so name and guest-base-handle lookup provide stable identical identity for the process lifetime.*

등록 전에 빈 이름, 빈 export 목록, alias/name/ordinal 중복, null handler, 지원하지 않는 ABI metadata, mapping 형태·범위·32비트 wrap 오류를 검증합니다. 기존 module과의 이름/alias/image range 충돌도 거절하며, 어떤 실패도 이미 등록된 module을 바꾸지 않습니다. 이 작업은 descriptor/lookup 기반만 제공하며 PE32 facade 생성, platform memory mapping, 실제 API handler나 resolver 이관을 포함하지 않습니다.

*Before registration, the code validates empty names and export lists, duplicate aliases/names/ordinals, null handlers, unsupported ABI metadata, and mapping shape/range/32-bit wrap errors. It also rejects name, alias, and image-range collisions with existing modules; no failure changes an already registered module. This task supplies only the descriptor and lookup foundation: PE32 facade generation, platform memory mapping, real API handlers, and resolver migration remain outside its scope.*

## 검증 / Validation

- Windows x86 Debug (`RE2DJ_WARNINGS_AS_ERRORS=ON`)에서 `re2dj_unit_tests`를 빌드하고 CTest를 통과했습니다: 1/1, `checks: 1853, failures: 0`.
- Windows x64 Debug에서 공용 core `re2dj_unit_tests`를 빌드하고 CTest를 통과했습니다: 1/1, `checks: 366, failures: 0`.
- Linux x64 Debug에서 `re2dj_unit_tests`를 빌드하고 CTest 및 직접 실행을 통과했습니다: 1/1, `checks: 1863, failures: 0`.
- Linux x86 Debug에서 `re2dj_unit_tests`를 빌드하고 CTest 및 직접 실행을 통과했습니다: 1/1, `checks: 1863, failures: 0`.
- 새 unit test는 alias 대소문자 정책, export name 대소문자 정책, name/ordinal thunk identity, invalid handle, descriptor/mapping 오류, duplicate와 overlap 거절, 실패 후 registry 불변성을 다룹니다.

*Validation performed:*

- *Built `re2dj_unit_tests` with Windows x86 Debug (`RE2DJ_WARNINGS_AS_ERRORS=ON`) and passed CTest: 1/1, `checks: 1853, failures: 0`.*
- *Built the shared-core `re2dj_unit_tests` with Windows x64 Debug and passed CTest: 1/1, `checks: 366, failures: 0`.*
- *Built `re2dj_unit_tests` with Linux x64 Debug and passed both CTest and direct execution: 1/1, `checks: 1863, failures: 0`.*
- *Built `re2dj_unit_tests` with Linux x86 Debug and passed both CTest and direct execution: 1/1, `checks: 1863, failures: 0`.*
- *The new unit test covers alias and export-name case policies, name/ordinal thunk identity, invalid handles, descriptor/mapping errors, duplicate and overlap rejection, and registry invariance after failure.*
