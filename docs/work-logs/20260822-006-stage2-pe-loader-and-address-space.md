# 작업 로그: Stage 2 PE 로더와 게스트 주소 공간

## 결과

Stage 2 이미지 적재 경로를 구현했습니다. `GuestAddress`는 32비트 값으로 host pointer와 분리했고, `AddressSpace`는 4 KiB 페이지 정렬 매핑, 접근 권한, little-endian 8/16/32비트 읽기·쓰기를 제공합니다. 겹치는 범위, 32비트 경계 초과, 범위 밖 접근을 거부합니다.

`LoadPe32Image()`는 PE32 헤더와 섹션을 매핑하고 zero-fill하며, 비선호 주소에서는 `IMAGE_REL_BASED_HIGHLOW` 재배치를 적용합니다. import descriptor와 thunk를 이름/ordinal 양쪽으로 해석하고 `0xF0000000`부터 16바이트 간격으로 합성 gate를 배정해 IAT에 기록합니다. 로드 실패는 호출자의 주소 공간과 gate 표를 변경하지 않습니다.

비실행 도구 `re2dj_pe_loader`를 추가해 파일 또는 사용자 지정 HDD 디렉터리의 게스트 경로를 적재하고 진입점, TLS directory, import gate를 출력하도록 했습니다.

## Result

Stage 2 image loading is implemented. `GuestAddress` remains a 32-bit value separate from host pointers, while `AddressSpace` provides 4 KiB page-aligned mappings, permissions, and little-endian 8/16/32-bit access. It rejects overlap, 32-bit overflow, and out-of-range access.

`LoadPe32Image()` maps and zero-fills PE32 headers and sections, applies `IMAGE_REL_BASED_HIGHLOW` relocations away from the preferred base, parses named and ordinal imports, assigns synthetic gates at 16-byte intervals from `0xF0000000`, and writes them to the IAT. A failed load preserves the caller's address space and gate table.

The non-executing `re2dj_pe_loader` tool loads either a direct file or a guest path under a user-supplied HDD directory and reports its entry point, TLS directory, and import gates.

## 원본 대상 확인

사용자 제공 1st SE HDD 디렉터리의 `ez2dj1.exe`를 적재했습니다.

* 선호 load base: `0x00400000`
* 계산된 entry point: `0x0043a640`
* TLS directory: 없음
* import: 7개 DLL, 144개 gate
* 재배치: `.reloc` 섹션 이름은 있으나 base relocation data directory가 비어 있어 비선호 주소 적재는 의도대로 거부됨

## Original-target check

The user-supplied 1st SE `ez2dj1.exe` loaded at preferred base `0x00400000`, produced entry point `0x0043a640`, reported no TLS directory, and bound 144 imports from seven DLLs. Loading it away from the preferred base was correctly rejected because its named `.reloc` section is not advertised by a base-relocation data directory.

## 검증

* `cmake --preset windows-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON` — 성공
* `cmake --build --preset windows-x64-debug` — 성공
* `ctest --preset windows-x64-debug` — 1/1 통과
* synthetic PE32 테스트 — 섹션 복사/zero-fill, HIGHLOW/ABSOLUTE 재배치, 이름/ordinal import, IAT gate, TLS 보고, 트랜잭션 실패 경로 통과
* 실제 `ez2dj1.exe` 적재 보고 — 144 imports 확인

Linux와 Web 호스트 빌드는 현재 Windows 작업 환경에 해당 toolchain이 없어 기존 TODO에 열린 항목으로 유지합니다.

## Verification

Windows x64 configure/build passed with warnings treated as errors, and CTest passed 1/1. Synthetic PE32 tests cover section copying and zero-fill, HIGHLOW/ABSOLUTE relocation, named and ordinal imports, IAT gates, TLS reporting, and transactional failure. The real `ez2dj1.exe` report confirmed all 144 imports. Linux and Web builds remain open in the existing TODO because their toolchains are unavailable in the current Windows environment.
