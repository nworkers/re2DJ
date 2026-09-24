# 작업 지시 341: 게스트 모듈 descriptor와 registry / Work order 341: Guest module descriptors and registry

설계: [게스트 모듈 descriptor와 registry](../design/20260921-341-guest-module-registry.md)

*Design: [Guest module descriptors and registry](../design/20260921-341-guest-module-registry.md)*

## 구현 순서 / Implementation sequence

1. `GuestExportDescriptor`, `GuestModuleDescriptor`, `GuestModuleMapping`과 등록 후 view를 선언합니다.
   *Declare `GuestExportDescriptor`, `GuestModuleDescriptor`, `GuestModuleMapping`, and registered views.*
2. module 이름/alias 정규화, descriptor와 mapping 검증, transactional registration을 구현합니다.
   *Implement module-name/alias normalization, descriptor and mapping validation, and transactional registration.*
3. 이름/handle module lookup과 이름/ordinal export lookup을 구현합니다.
   *Implement name/handle module lookup and name/ordinal export lookup.*
4. duplicate, invalid handle, bounds, lookup case policy와 실패 불변식 단위 테스트를 추가합니다.
   *Add unit tests for duplicates, invalid handles, bounds, lookup case policy, and failure invariants.*
5. CMake와 누적 architecture/TODO 문서를 갱신하고 Linux x64/x86 및 Windows x86에서 빌드·테스트합니다.
   *Update CMake and cumulative architecture/TODO documents, then build and test on Linux x64/x86 and Windows x86.*
6. 작업 로그와 Git commit을 남깁니다.
   *Leave a work log and Git commit.*

## 완료 조건 / Completion criteria

- module alias는 ASCII case-insensitive이고 export name은 case-sensitive입니다.
- 같은 module이 이름과 guest base handle로 동일한 등록 객체에 해석됩니다.
- export name과 ordinal은 동일한 thunk address와 handler metadata로 해석됩니다.
- invalid descriptor/mapping/duplicate/range overlap은 registry를 변경하지 않고 실패합니다.
- PE image 생성, platform mapping과 실제 API 의미는 포함하지 않습니다.

*Completion requires ASCII case-insensitive module aliases, case-sensitive export names, identical registered-module identity by name and guest base handle, name/ordinal export resolution to the same thunk and handler metadata, transactional rejection of invalid descriptors/mappings/duplicates/overlaps, and no PE generation, platform mapping, or API semantics in this task.*
