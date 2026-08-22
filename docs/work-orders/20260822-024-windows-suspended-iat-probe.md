# Windows Suspended IAT Probe

## 한국어

1. 원본 PE32 파일에서 import descriptor, lookup thunk, IAT RVA를 읽기 전용으로 해석합니다.
2. target profile로 원본 EXE를 `CREATE_SUSPENDED`로 생성하고 WOW64 PEB에서 main image base를 확인합니다.
3. 각 IAT slot을 원격 읽어 nonzero 외부 함수 주소인지 확인하고 module별 개수를 보고합니다.
4. child를 resume하지 않고 종료합니다.
5. Windows build·unit test·실제 HDD probe 결과를 작업 로그에 남깁니다.

## English

1. Read-only parse import descriptors, lookup thunks, and IAT RVAs from the original PE32 file.
2. Create the original EXE with `CREATE_SUSPENDED` through the target profile and confirm its main-image base through the WOW64 PEB.
3. Read each IAT slot remotely, verify that it is a nonzero external function address, and report counts by module.
4. Terminate the child without resuming it.
5. Record Windows build, unit-test, and live HDD probe results in the work log.
