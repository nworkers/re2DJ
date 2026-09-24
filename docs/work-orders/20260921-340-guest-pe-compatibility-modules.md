# 작업 지시 340: 게스트 PE 호환 모듈 계획 / Work order 340: Guest PE compatibility module plan

## 목표 / Objective

현재 Linux i386 진단의 pseudo `kernel32` handle과 API별 resolver 조건문을, DLL별 독립 명세·실제 PE32 export facade·공용 module registry 구조로 이관할 구현 계획을 확정합니다. 이번 작업 지시는 설계와 후속 작업 분할까지만 승인하며 코드 구현은 시작하지 않습니다.

*Finalize an implementation plan that migrates the current Linux i386 diagnostic's pseudo-`kernel32` handle and per-API resolver conditionals into independent per-DLL declarations, real PE32 export facades, and a shared module registry. This work order authorizes design and follow-up decomposition only; it does not begin code implementation.*

기준 설계는 [게스트 PE 호환 모듈 설계](../design/20260921-340-guest-pe-compatibility-modules.md)입니다.

*The normative design is [Guest PE compatibility module design](../design/20260921-340-guest-pe-compatibility-modules.md).*

## 후속 작업 단위 / Follow-up work units

1. **341 — module descriptor와 registry**
   - `GuestModuleDescriptor`, `GuestExportDescriptor`, `GuestModuleRegistry`를 플랫폼 중립 코드로 추가합니다.
   - module 이름/별칭, export 이름/ordinal, duplicate와 invalid-handle 단위 테스트를 추가합니다.
   - PE 생성이나 실제 원본 실행은 포함하지 않습니다.

   ***341 — module descriptors and registry***
   - *Add platform-neutral `GuestModuleDescriptor`, `GuestExportDescriptor`, and `GuestModuleRegistry` code.*
   - *Add unit tests for module names/aliases, export names/ordinals, duplicates, and invalid handles.*
   - *Do not include PE generation or real-original execution.*

2. **342 — PE32 facade builder**
   - descriptor에서 최소 유효 PE32 export image를 생성합니다.
   - 독립 parser와 synthetic fixture로 headers, sections, export tables, thunk RVA와 bounds를 검증합니다.
   - 생성 binary는 커밋하지 않습니다.

   ***342 — PE32 facade builder***
   - *Generate a minimally valid PE32 export image from descriptors.*
   - *Validate headers, sections, export tables, thunk RVAs, and bounds with an independent parser and synthetic fixture.*
   - *Do not commit generated binaries.*

3. **343 — `kernel32` module과 Linux i386 mapping**
   - `kernel32_module.h/.cpp`에 확인된 `GetModuleHandleA`, `GetProcAddress`, `GetVersion`, `CreateFileA`를 선언합니다.
   - facade를 충돌 없는 guest address에 RW staging 후 R/RX로 mapping하고 import bridge에 연결합니다.
   - 실제 API 의미 중 이번 단계는 기존 진단 반환만 보존합니다.

   ***343 — `kernel32` module and Linux i386 mapping***
   - *Declare the confirmed `GetModuleHandleA`, `GetProcAddress`, `GetVersion`, and `CreateFileA` exports in `kernel32_module.h/.cpp`.*
   - *Map the facade at a collision-free guest address using RW staging followed by R/RX protection, then connect it to the import bridge.*
   - *Preserve only the existing diagnostic returns for API semantics in this stage.*

4. **344 — resolver 이관과 실제 CHD 회귀**
   - `0x7F000001` pseudo handle과 `native_create_file_observation.cpp`의 API별 lookup을 제거합니다.
   - static IAT와 dynamic resolver의 동일 thunk address를 검증합니다.
   - 실제 4th CHD에서 `kernel32` base, `GetVersion`/`CreateFileA` address, `\\.\NTICE` 호출 ABI를 확인합니다.

   ***344 — resolver migration and real-CHD regression***
   - *Remove the `0x7F000001` pseudo handle and per-API lookup from `native_create_file_observation.cpp`.*
   - *Verify identical thunk addresses for static IAT and dynamic resolution.*
   - *Against the real 4th CHD, confirm the `kernel32` base, `GetVersion`/`CreateFileA` addresses, and the `\\.\NTICE` call ABI.*

5. **후속 — handle/VFS, 다른 DLL, x64/Windows**
   - guest handle/VFS service는 별도 설계로 `CreateFileA` 의미를 구현합니다.
   - `user32`, `gdi32`, `ddraw`, `dsound`는 확인된 첫 export 순서대로 각각 별도 작업을 만듭니다.
   - Linux x64 compatibility-mode trampoline과 Windows WoW64 adapter는 서로 독립된 작업으로 검증합니다.

   ***Follow-ups — handles/VFS, other DLLs, and x64/Windows***
   - *Implement `CreateFileA` semantics through a separately designed guest-handle/VFS service.*
   - *Create separate tasks for `user32`, `gdi32`, `ddraw`, and `dsound` in the order their first exports are confirmed.*
   - *Validate the Linux x64 compatibility-mode trampoline and Windows WoW64 adapter as independent tasks.*

## 공통 제약 / Common constraints

- 원본 실행 파일과 자산은 수정하거나 저장소에 추가하지 않습니다.
  *Do not modify or add original executables or assets to the repository.*
- Wine 코드를 복사하거나 링크하지 않고 공개 설계 자료만 참고합니다.
  *Do not copy or link Wine code; use only public design material as reference.*
- 공용 core에 host OS API와 host pointer를 넣지 않습니다.
  *Do not put host OS APIs or host pointers in the shared core.*
- Windows 배포물로 `kernel32.dll`, `user32.dll` 같은 shadowing 파일을 만들지 않습니다.
  *Do not produce shadowing files such as `kernel32.dll` or `user32.dll` in Windows output.*
- 확인되지 않은 export를 성공 stub으로 등록하지 않습니다.
  *Do not register unconfirmed exports as success stubs.*

## 각 구현 작업의 완료 조건 / Completion criteria for each implementation task

각 후속 구현 작업은 설계·작업 지시·코드·단위 또는 synthetic 검증·영향받는 Linux x86/x64 및 Windows build 검증·작업 로그·Git 커밋을 같은 작업 단위에 남겨야 합니다. 실제 CHD가 필요한 단계는 사용자 제공 경로를 읽기 전용으로 사용하고 확인됨/추정/미확정을 관련 분석 문서에 반영합니다.

*Each follow-up implementation task must leave its design, work order, code, unit or synthetic validation, affected Linux x86/x64 and Windows build validation, work log, and Git commit in the same task unit. A stage requiring a real CHD uses the user-supplied path read-only and updates the relevant analysis document with confirmed/inferred/unresolved status.*
