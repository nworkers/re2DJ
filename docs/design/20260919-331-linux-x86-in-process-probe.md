# Linux x86 in-process runner probe / Linux x86 in-process runner probe

## 목적 / Purpose

Linux x86 `NativeInProcessRunner`가 IPC 없이 synthetic PE의 import thunk를 handler로 전달하고, handler의 register·stack cleanup 결과로 guest entry를 계속 실행함을 검증한다.

*Verify that Linux x86 `NativeInProcessRunner` delivers a synthetic PE import thunk to its handler without IPC and resumes guest entry using the handler's register and stack-cleanup result.*

## 계약 / Contract

기존 native IPC probe와 같은 relocatable PE fixture를 사용한다. fixture는 named 및 ordinal import를 순서대로 호출한다. handler는 event stack start의 첫 argument를 읽어 `41 -> EAX 42`, `42 -> EAX 43, EDX 1`을 반환하며 각 호출에서 4 bytes를 callee-cleanup한다. guest entry는 두 결과와 TLS state를 합산하여 51로 종료한다.

*Use the same relocatable PE fixture as the existing native IPC probe. The fixture calls named and ordinal imports in sequence. The handler reads the first argument at event stack start, returns `41 -> EAX 42` and `42 -> EAX 43, EDX 1`, and callee-cleans four bytes for each call. Guest entry adds both results and TLS state to exit with 51.*

별도 fixture entry의 invalid instruction은 runner가 `NativeGuestFault`를 반환하는지 확인한다. 이 검증은 actual Win32 API binding이나 CLI backend 선택을 뜻하지 않는다.

*A separate invalid instruction in the fixture entry verifies that the runner returns `NativeGuestFault`. This validation does not establish actual Win32 API binding or CLI backend selection.*
