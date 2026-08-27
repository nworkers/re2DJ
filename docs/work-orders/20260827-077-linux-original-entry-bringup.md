# Linux 원본 entry bring-up 작업 지시

## 상태

**완료.** 전체 방향은 [Linux 원본 실행 경로 설계](../design/20260827-077-linux-original-execution.md)에 두며, 이 작업 단위는 production helper와 CLI를 연결해 원본의 첫 실행 경계를 관찰하는 데 한정한다.

## 목표

Linux `re2dj --run`이 사용자가 지정한 target의 PE32 bytes를 i386 helper에 전달하고 원본 entry를 실행한 뒤, 첫 import gate·fault·process exit 중 하나를 이름과 guest context가 포함된 통제 결과로 보고하게 한다.

## 작업

1. Linux i386 helper source/target에서 probe 전용 이름을 제거하고 production 실행 target으로 승격한다.
2. 기존 synthetic x86-64/i386 integration probe가 production helper를 계속 검증하도록 갱신한다.
3. Linux host orchestration을 전용 platform 파일로 분리하고 target executable 읽기, `PrepareImage`, `Start`와 event loop를 연결한다.
4. CLI에 Linux helper 경로를 받는 옵션과 `--run` 연결을 추가한다. 다른 host의 기존 동작은 유지한다.
5. 첫 import는 `LoadedPeImage.imports` metadata로 module/name 또는 ordinal을 찾아 통제된 미구현 경계로 보고한다. fault와 정상 exit도 구분한다.
6. Linux x64 build·CTest, i386 helper build와 synthetic IPC integration을 검증한다. 원본 HDD 실행은 사용자가 제공한 경로로만 수행한다.
7. architecture, porting plan, TODO와 작업 로그를 갱신하고 작업 커밋을 남긴다.

## 완료 기준

- `re2dj --hdd <dir> --target <id> --run --linux-helper <path>`가 Linux에서 production helper를 시작한다.
- helper는 선택한 image의 선호 base와 entry를 사용하며 첫 import/fault/exit를 host에 보고한다.
- 미구현 import는 host crash나 무정의 반환으로 진행하지 않고 module/function과 EIP/ESP를 출력한 뒤 종료한다.
- synthetic PE32 integration의 relocation, TLS, named/ordinal import와 result 51 검증이 유지된다.
- 원본 자산 없이 Linux/Windows 단위 테스트가 통과한다.

---

# Linux Original Entry Bring-up Work Order

## Status

**Complete.** The complete direction lives in the [Linux original-executable design](../design/20260827-077-linux-original-execution.md). This task unit is limited to connecting the production helper and CLI so the first original execution boundary can be observed.

## Goal

Have Linux `re2dj --run` send the selected target's PE32 bytes to the i386 helper, execute the original entry point, and report the first import gate, fault, or process exit as a controlled result with its name and guest context.

## Tasks

1. Remove probe-only naming from the Linux i386 helper source/target and promote it to a production executable.
2. Keep the synthetic x86-64/i386 integration probe validating the production helper.
3. Add platform-specific Linux host orchestration for target reading, `PrepareImage`, `Start`, and the event loop.
4. Add a Linux helper-path CLI option and connect `--run`, preserving existing behavior on other hosts.
5. Resolve the first gate through `LoadedPeImage.imports`; report unimplemented imports, faults, and exits distinctly.
6. Verify Linux x64 build/CTest, the i386 build, and synthetic IPC integration. Run originals only from a user-supplied HDD path.
7. Update architecture, the porting plan, TODO, and the work log, then commit the task.

## Completion Criteria

Linux starts the production helper through `re2dj --hdd <dir> --target <id> --run --linux-helper <path>`, reports the selected image's first import/fault/exit without undefined continuation, retains the synthetic relocation/TLS/named-and-ordinal-import result-51 integration, and keeps asset-free Linux/Windows tests green.
