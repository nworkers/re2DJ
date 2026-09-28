# 작업 404 작업 지시서 — 게스트 예외 디스패치와 Linux x86 호스트 GS 복원 / Task 404 work order — Guest exception dispatch and Linux x86 host GS restoration

## 작업 개요 / Task Overview

- 대상: Linux x86/x64 게스트 예외 디스패치 및 Linux x86 호스트 GS 세그먼트 레지스터 복원
- 목표:
  1. Linux 환경에서 `ez2dj1st` 등 보호 스텁의 예외를 게스트 SEH 체인으로 전달하여 실행 지속.
  2. `kernel32!RtlUnwind` HLE 구현을 통한 게스트 예외 프레임 정리 지원.
  3. Linux x86(i386) 환경에서 게스트의 `%gs` 수정으로 인해 시그널 핸들러 및 import bridge 진입 시 발생하는 coredump 크래시 완전 해결.

*Scope: Linux x86/x64 guest exception dispatch and Linux x86 host GS segment register restoration.*
*Goals:*
*1. Pass exceptions from protection stubs such as `ez2dj1st` down the guest SEH chain on Linux to sustain execution.*
*2. Support guest exception frame unwind via `kernel32!RtlUnwind` HLE.*
*3. Completely resolve coredump crashes on Linux x86 (i386) caused by guest `%gs` modifications upon entering the signal handler and import bridge.*

---

## 작업 항목 / Tasks

1. **호스트 GS 셀렉터 및 시그널 핸들러 트램펄린 구현**:
   - `src/platform/linux/x86/native_process_bootstrap.cpp`:
     - 전역 `g_host_gs_selector` 선언 및 프로세스/초기화 시점 값 보관.
     - naked 트램펄린 `NativeGuestSignalTrampoline` 구현: PC 상대 주소로 `g_host_gs_selector`를 읽어 `%gs`를 복원한 뒤 `GuestSignalHandler`로 점프.
     - `action.sa_sigaction`에 `NativeGuestSignalTrampoline` 등록.
     - `CallGuestEntry`, `CallGuestTls` 복귀 직후 호스트 GS 복원.
     - `NativeProcessBootstrap::Impl::Execute`에서 호스트 GS 복원 보장.

2. **Import Gate 트램펄린 및 stdcall 호출자 GS 복원**:
   - `src/platform/linux/x86/native_import_bridge.cpp`:
     - `NativeImportGateBridge`를 naked 트램펄린으로 변경하여 게스트 GS 보존 -> 호스트 GS 로드 -> `NativeImportGateBridgeImpl` 호출 -> 게스트 GS 복원 -> `ret $4`.
     - `CallGuestStdcallWords` 복귀 직후 호스트 GS 복원.

3. **게스트 예외 전달 및 RtlUnwind HLE 점검**:
   - `src/platform/linux/native_guest_seh.cpp`, `native_guest_seh.h`, `kernel32_module.cpp`의 미커밋 변경사항 확인 및 정합성 검증.

4. **빌드 및 회귀 검증**:
   - Linux x86 및 x64 단위 테스트 전체 빌드 및 테스트 통과.
   - Linux x86에서 `re2dj --run ez2dj1st --call-limit 50` 실행으로 coredump 해소 및 정상 50호출 도달 확인.
   - Linux x64 및 ez2dj4th 무회귀 확인.

5. **문서화 및 작업 로그 작성**:
   - `docs/TODO.md` 갱신.
   - `docs/work-logs/20260927-404-guest-exception-dispatch.md` 작성.
