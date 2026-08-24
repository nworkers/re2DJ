# 레거시 I/O 포트 HLE 작업 지시

관련 설계: [레거시 I/O 포트 HLE](../design/20260825-062-legacy-io-port-hle.md)

## 상태

**완료.** 원본 port 호출과 소비 구조, 공용 raw bus, Windows x86 instruction trap과 반복 canonical 검증을 완료했다.

## 작업 범위

1. platform-neutral `LegacyIoPortBus`에 확인된 byte port와 idle/raw 상태를 정의한다.
2. 알 수 없는 port와 width를 명시적으로 거부하고 byte output의 마지막 값을 보존한다.
3. Windows x86 launcher에서 확인된 `in al,dx`와 `out dx,al` 주소만 `EXCEPTION_PRIV_INSTRUCTION`으로 처리한다.
4. 처리한 read/write를 구조화된 진단 event로 기록한다.
5. 공용 단위 테스트, Windows x86 빌드/CTest, canonical 2회 실행을 수행한다.
6. 다음 최초 실패와 access violation 유무를 analysis, architecture, TODO, 작업 로그에 반영한다.

## 완료 조건

기존 port `0x103` read의 privileged-instruction 예외가 사라지고, 확인된 I/O만 처리하며, 두 canonical 실행이 동일한 다음 경계에 도달해야 한다. 기존 또는 새로운 `av_access`가 있으면 주소와 접근 종류를 기록한다.

---

# Legacy I/O Port HLE Work Order

Related design: [Legacy I/O Port HLE](../design/20260825-062-legacy-io-port-hle.md)

## Status

**Complete.** The original port callers and consumers, shared raw bus, Windows x86 instruction trap, and repeated canonical verification are complete.

## Scope and completion

Define confirmed byte ports and idle state in a platform-neutral bus, reject unknown accesses, retain raw output bytes, handle only the confirmed `in al,dx` and `out dx,al` helper addresses in the Windows x86 launcher, and emit structured diagnostics. Unit tests, a Windows x86 build/CTest, and two canonical runs must remove the old port exception and reproduce one next boundary while continuously checking access violations.
