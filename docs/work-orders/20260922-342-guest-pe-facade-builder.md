# 작업 지시 342: 게스트 PE32 export facade builder / Work order 342: Guest PE32 export facade builder

설계: [게스트 PE32 export facade builder](../design/20260922-342-guest-pe-facade-builder.md)

*Design: [Guest PE32 export facade builder](../design/20260922-342-guest-pe-facade-builder.md)*

## 구현 순서 / Implementation sequence

1. facade build options와 file-image 결과 형식을 선언합니다.
   *Declare facade build options and the file-image result type.*
2. descriptor와 32비트 address/count/string 입력을 검증하고 deterministic ordinal을 배정합니다.
   *Validate descriptor and 32-bit address/count/string inputs, then assign deterministic ordinals.*
3. checked alignment/layout 계산으로 DOS/PE32 header와 `.edata` export tables를 생성합니다.
   *Generate DOS/PE32 headers and `.edata` export tables using checked alignment and layout arithmetic.*
4. descriptor 순서로 `.text` 19-byte bridge thunk를 생성하고 result metadata를 채웁니다.
   *Generate `.text` 19-byte bridge thunks in descriptor order and populate result metadata.*
5. 독립 test parser로 header, section, export table, ordinal/name 정책과 thunk bytes를 검증합니다.
   *Verify headers, sections, export tables, ordinal/name policy, and thunk bytes with an independent test parser.*
6. CMake, PE32 지식 문서, architecture와 TODO를 갱신하고 Windows/Linux x86/x64 build와 CTest를 실행합니다.
   *Update CMake, PE32 knowledge, architecture, and TODO, then run Windows/Linux x86/x64 builds and CTest.*
7. 작업 로그와 Git commit을 남깁니다.
   *Leave a work log and Git commit.*

## 완료 조건 / Completion criteria

- 생성 image가 i386 PE32 DLL이며 `.edata`와 `.text` 경계가 일관됩니다.
- name/ordinal export가 동일 descriptor thunk를 가리키고 name table이 정렬됩니다.
- sparse ordinal, ordinal-only와 name-only export를 정확히 표현합니다.
- thunk immediate와 relative call target이 입력 gate/bridge/cleanup 주소와 일치합니다.
- malformed input은 output을 바꾸지 않고 실패합니다.
- host mapping, page protection 적용, `kernel32` 실제 descriptor와 resolver 이관은 포함하지 않습니다.

*Completion requires a consistent i386 PE32 DLL image with `.edata`/`.text` separation, name and ordinal exports pointing at the same descriptor thunk, a sorted name table, correct sparse/ordinal-only/name-only representation, thunk immediates matching gate/bridge/cleanup inputs, and transactional failure for malformed input. Host mapping, page-protection application, the real `kernel32` descriptor, and resolver migration remain outside this task.*
