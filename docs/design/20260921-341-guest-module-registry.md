# 작업 341: 게스트 모듈 descriptor와 registry / Task 341: Guest module descriptors and registry

## 목표 / Objective

게스트가 보는 DLL 이름, export metadata와 mapping identity를 host pointer 없이 보존하는 플랫폼 중립 `GuestModuleRegistry`를 구현합니다. 이 단계는 PE32 image 생성이나 플랫폼 mapping을 포함하지 않으며, 이후 facade builder와 `GetModuleHandleA`·`GetProcAddress`가 공유할 단일 lookup 계약을 고정합니다.

*Implement a platform-neutral `GuestModuleRegistry` that preserves guest-visible DLL names, export metadata, and mapped identity without host pointers. This stage does not generate or map PE32 images; it fixes the single lookup contract later shared by the facade builder, `GetModuleHandleA`, and `GetProcAddress`.*

## 자료구조 / Data model

`GuestExportDescriptor`는 export name, 선택적 ordinal, x86 호출 규약, 인자 수와 기존 `ImportHandler` identity를 가집니다. name과 ordinal 중 하나 이상이 있어야 하고 handler는 null일 수 없습니다. export name lookup은 Win32 계약처럼 대소문자를 구분하며 ordinal lookup은 정확한 16비트 값으로 수행합니다.

*`GuestExportDescriptor` carries an export name, optional ordinal, x86 calling convention, argument count, and existing `ImportHandler` identity. At least one of name or ordinal is required, and the handler cannot be null. Export-name lookup is case-sensitive like the Win32 contract; ordinal lookup uses the exact 16-bit value.*

`GuestModuleDescriptor`는 canonical module name, 명시적 alias 목록과 export 목록을 가집니다. module name과 alias lookup만 ASCII 대소문자를 무시합니다. `.dll`을 무조건 붙이는 암묵 규칙은 두지 않고 `kernel32`·`kernel32.dll`처럼 지원할 이름을 descriptor에 명시합니다.

*`GuestModuleDescriptor` carries a canonical module name, explicit aliases, and exports. Only module-name and alias lookup is ASCII case-insensitive. The registry does not implicitly append `.dll`; supported forms such as `kernel32` and `kernel32.dll` are declared explicitly in the descriptor.*

`GuestModuleMapping`은 module base, image size와 descriptor export 순서에 대응하는 thunk guest address 목록을 가집니다. 등록된 module/export view는 descriptor와 mapping을 registry가 소유한 값으로 복사해 process lifetime 동안 안정된 identity를 제공합니다.

*`GuestModuleMapping` carries the module base, image size, and thunk guest addresses corresponding to descriptor-export order. Registered module/export views copy descriptor and mapping values into registry-owned storage, providing stable identity for process lifetime.*

## 검증 규칙 / Validation rules

- canonical name과 alias는 비어 있을 수 없고 한 module 안이나 이미 등록된 module과 ASCII case-insensitive로 충돌할 수 없습니다.
- export는 name 또는 ordinal을 하나 이상 가지며 name·ordinal 중복을 각각 거절합니다.
- 호출 규약은 `kStdcall` 또는 `kCdecl`, 인자 수는 `ImportDispatcher::kMaximumArgumentCount` 이하여야 합니다.
- module base와 image size는 0이 아니고 32비트 주소 공간에서 wrap하지 않아야 합니다.
- thunk 수는 export 수와 같고 모든 thunk address는 `[base, base + image_size)` 안에 있어야 합니다.
- 등록된 image range는 다른 module과 겹칠 수 없습니다.

*Canonical names and aliases must be non-empty and cannot collide case-insensitively within one module or with a registered module. Each export has at least a name or ordinal, with duplicate names and ordinals rejected independently. Calling convention and argument count follow `ImportDispatcher`. Module base and size are nonzero and non-wrapping, thunk count matches export count, every thunk lies inside the mapped image, and registered image ranges cannot overlap.*

등록은 전체 검증이 끝난 뒤 한 번에 반영하는 transactional operation입니다. 실패 시 기존 registry를 바꾸지 않습니다.

*Registration is transactional: validate everything before committing the module, leaving the existing registry unchanged on failure.*

## lookup 계약 / Lookup contract

registry는 module을 이름/alias 또는 exact base handle로 찾고, handle과 export name/ordinal로 등록된 export를 찾습니다. 알 수 없는 이름, invalid handle, 존재하지 않는 export는 null을 반환합니다. 이 단계는 Win32 last-error나 로그를 만들지 않으며, 이후 API binding이 해당 실패 정책을 결정합니다.

*The registry finds modules by name/alias or exact base handle and resolves registered exports by handle plus name/ordinal. Unknown names, invalid handles, and missing exports return null. This stage neither sets Win32 last-error nor logs; later API bindings decide those failure policies.*

## 파일과 검증 / Files and validation

```text
include/re2dj/hle/modules/guest_module.h
include/re2dj/hle/modules/guest_module_registry.h
src/hle/modules/guest_module_registry.cpp
tests/unit/guest_module_registry_test.cpp
```

단위 테스트는 module alias의 ASCII case-insensitive lookup, export name의 case-sensitive lookup, name/ordinal resolution, invalid handle, 모든 duplicate/invalid descriptor, mapping bounds와 transactional failure를 확인합니다. Linux x64·x86와 Windows x86 warnings-as-errors build 및 단위 테스트를 실행합니다.

*Unit tests cover ASCII case-insensitive module aliases, case-sensitive export names, name/ordinal resolution, invalid handles, duplicate and invalid descriptors, mapping bounds, and transactional failure. Run Linux x64/x86 and Windows x86 warnings-as-errors builds and unit tests.*
