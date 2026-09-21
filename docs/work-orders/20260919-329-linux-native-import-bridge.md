# 작업 지시 329: Linux native import bridge 분리 / Work order 329: Linux native import bridge extraction

## 목표 / Goal

Linux i386 helper의 import thunk bridge를 재사용 가능한 동기 handler 경계로 분리한다.

*Extract the Linux i386 helper import-thunk bridge into a reusable synchronous-handler boundary.*

## 작업 / Work

1. `NativeImportGateEvent`, result, handler configuration과 i386 bridge entry를 전용 source로 둔다.
2. thunk binding이 새 bridge와 cleanup storage 주소를 사용하게 한다.
3. IPC protocol loop를 bridge handler로 옮기고 기존 protocol message 형식을 유지한다.
4. architecture 문서에 현재 구현된 bridge 구성을 기록한다.
5. x86-64/x86 helper probe 전체 script를 실행한다.

*1. Put `NativeImportGateEvent`, result, handler configuration, and the i386 bridge entry in dedicated sources.
2. Make thunk binding use the new bridge and cleanup-storage addresses.
3. Move the IPC protocol loop into a bridge handler while retaining the existing protocol message format.
4. Record the implemented bridge structure in the architecture document.
5. Run the full helper-probe script for x86-64 and x86.*

## 완료 기준 / Done criteria

기존 fixture의 정상 import, fault, stop, capability rejection 결과가 유지되고, helper의 IPC adapter가 bridge ABI 계산을 직접 수행하지 않는다.

*Existing fixture results for normal import, fault, stop, and capability rejection remain intact, and the helper IPC adapter no longer calculates the bridge ABI directly.*
