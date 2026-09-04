# Task 164: EZ2DJ 4th 실패 가상 호출 대상 특정

## 작업 목표

IAT 슬롯 `0x006d1908`과 `0x006d1724`의 import API 이름을 확정하고, 가상 호출 지점 `0x00010a6f`와 반환 지점 `0x00010a72`에서 인터페이스 포인터, 가상 함수 포인터 주소, 전달 인자, 반환값을 관찰하여 실패 원인과 대상을 규명합니다.

## 선행 문서

- [Task 164 설계](../design/20260903-164-ez2dj4th-failing-virtual-call-target.md)
- [Task 163 작업 로그](../work-logs/20260903-163-ez2dj4th-guard-failure-source.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. `iat_verifier`에 IAT 슬롯 RVA를 통해 module/symbol 이름을 역방향 조회하는 `ResolveIatSlot` 기능 및 단위 테스트 추가.
2. launcher probe 실행 시 `0x006d1908` 및 `0x006d1724` 슬롯을 해석하여 진단 로그에 기록.
3. launcher probe의 entry trace / hardware breakpoint 대상을 다음 네 곳으로 변경:
   - `0x00010a6f`: 가상 호출 직전 (`virtual_call_site`). `ecx`, `edx`, `[ecx+0x54]`, `[esp]` 관찰.
   - `0x00010a72`: 가상 호출 직후 (`virtual_call_return`). `eax` 관찰.
   - `0x00010975`: 실패 함수 진입부 (`failing_func_entry`).
   - `0x000107d9`: guard 2 callee 내부 호출 지점 (`guard2_call_site`).
4. 프로세스 모듈 목록을 바탕으로 가상 함수 주소(`[ecx+0x54]`)의 소속 DLL 및 심볼 매핑 로깅.
5. 설계·작업 로그 및 런타임 분석 문서 갱신.

## 비범위

- field 값 직접 주입 또는 원본 코드 패치
- Hardlock 응답 material 조작
- COM 인터페이스 임의 모킹 또는 동작 변조
- 오류 메시지 문자열 내용 기록

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

실제 CHD가 있으면 확장 idle 경계와 함께 실행하고, 가상 호출 지점과 반환 지점에서 수집된 레지스터 값 및 모듈 매핑 결과를 검증합니다.

---

# Task 164: EZ2DJ 4th Failing Virtual Call Target

## Objective

Establish the imported API names for IAT slots `0x006d1908` and `0x006d1724`, and observe the interface pointer, virtual function pointer address, argument, and return value at the virtual call site `0x00010a6f` and return site `0x00010a72` to identify the failing target and cause.

## Preceding Documents

- [Task 164 design](../design/20260903-164-ez2dj4th-failing-virtual-call-target.md)
- [Task 163 work log](../work-logs/20260903-163-ez2dj4th-guard-failure-source.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation Scope

1. Add `ResolveIatSlot` to `iat_verifier` to reverse-lookup module and symbol names by IAT slot RVA, along with unit tests.
2. Resolve slots `0x006d1908` and `0x006d1724` during launcher probe startup and log them as diagnostic events.
3. Configure the launcher probe's entry trace / hardware breakpoints for four sites:
   - `0x00010a6f`: Virtual call site (`virtual_call_site`). Observe `ecx`, `edx`, `[ecx+0x54]`, `[esp]`.
   - `0x00010a72`: Virtual call return (`virtual_call_return`). Observe `eax`.
   - `0x00010975`: Failing function entry (`failing_func_entry`).
   - `0x000107d9`: Guard 2 caller site (`guard2_call_site`).
4. Log target module and symbol mapping for the virtual function address (`[ecx+0x54]`) using loaded module information.
5. Update design, work log, and runtime analysis documents.

## Out of Scope

- Direct field injection or patching original binary code.
- Manipulating Hardlock response materials.
- Arbitrary mocking or mutating COM interface behavior.
- Recording error message text contents.

## Minimum Verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the real CHD is available, run with the extended idle boundary and verify the collected registers and module mapping at the virtual call and return sites.
