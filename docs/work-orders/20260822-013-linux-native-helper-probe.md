# 작업 지시: Linux native x86 helper 최소 prototype

## 목표

WSL Linux x86-64 host와 별도 i386 helper 사이에서 실제 x86 gate event/reply 및 stack memory IPC를 검증합니다.

## Goal

Validate a real x86 gate event/reply and stack-memory IPC between a WSL Linux x86-64 host and separate i386 helper.

## 작업 항목

1. OS 독립 native helper protocol header를 `src/platform/`로 이동하고 Windows include를 갱신합니다.
2. Linux i386 helper에 `__stdcall` gate, 제한된 memory read/write와 completion loop를 구현합니다.
3. Linux x64 host probe에 pipe, `fork`/`exec`, packet 검증과 child 정리를 구현합니다.
4. Linux x64 및 i386 helper용 CMake target/preset을 추가합니다.
5. WSL에서 두 build와 integration을 수행하는 반복 가능 script를 추가합니다.
6. 아키텍처, 포팅 계획, README, TODO와 작업 로그를 갱신합니다.
7. Windows 회귀 build/probe와 Linux x64 unit/integration 검증 후 커밋합니다.

## Work items

1. Move the OS-independent native-helper protocol header to `src/platform/` and update Windows includes.
2. Implement an i386 Linux helper with a `__stdcall` gate, bounded memory read/write, and completion loop.
3. Implement pipes, `fork`/`exec`, packet validation, and child cleanup in the Linux x64 host probe.
4. Add CMake targets/presets for Linux x64 and the i386 helper.
5. Add a repeatable WSL script that builds both sides and runs integration.
6. Update architecture, porting plan, READMEs, TODO, and the work log.
7. Run Windows regression builds/probes and Linux x64 unit/integration verification, then commit.

## 완료 조건

Linux host probe가 i386 helper의 gate argument 41을 IPC로 읽고 쓴 뒤 EAX 42를 반환하며 process result 42와 child exit 0을 확인하면 완료입니다.

## Completion criteria

The task is complete when the Linux host probe reads and writes gate argument 41 from the i386 helper over IPC, returns EAX 42, and observes process result 42 with child exit zero.
