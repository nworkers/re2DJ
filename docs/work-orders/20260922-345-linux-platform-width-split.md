# 작업 지시 345 — Linux 플랫폼 비트 폭 분리 / Work order 345 — Splitting the Linux platform tree by bit width

설계: [20260922-345-linux-platform-width-split.md](../design/20260922-345-linux-platform-width-split.md)
선행: [작업 347 플랫폼 비트 폭 디렉터리 규칙](20260921-347-platform-architecture-directories.md)

## 목표 / Objective

작업 347의 규칙을 `src/platform/linux/`에 적용한다. i386 전용 구현을 `src/platform/linux/x86/`로 옮기고, 두 폭에서 빌드되는 코드만 루트에 남긴다. 동작은 바꾸지 않는다.

*Apply the Task 347 rule to `src/platform/linux/`: move i386-only implementations into `src/platform/linux/x86/` and leave only dual-width code at the root, with no behavior change.*

## 작업 / Work

1. 설계의 이동 대상 표에 있는 파일을 `src/platform/linux/x86/`로 옮긴다. Git이 rename으로 인식하도록 내용은 바꾸지 않고 옮긴다.
2. 옮긴 파일과 루트에 남은 파일의 `#include`를 새 경로에 맞춘다.
3. `CMakeLists.txt`의 Linux source list를 새 경로로 갱신한다. 제품 빌드와 `RE2DJ_BUILD_LINUX_NATIVE_HELPER` 빌드 양쪽을 본다.
4. `ARCHITECTURE.md`와 `docs/TODO.md`를 갱신한다.
5. 빌드와 테스트를 수행하고 작업 로그를 남긴다.

*Move the files listed in the design into `src/platform/linux/x86/` without content changes so Git records renames; fix includes on both sides; update the Linux source lists in `CMakeLists.txt` for the product build and the `RE2DJ_BUILD_LINUX_NATIVE_HELPER` build; update `ARCHITECTURE.md` and `docs/TODO.md`; build, test, and leave a work log.*

## 제약 / Constraints

* **동작을 바꾸지 않는다.** 이 작업은 파일 위치와 `#include`만 바꾼다. 함수 본문, 조건, 로그 문구를 고치지 않는다.
* `native_create_file_observation.cpp`와 `original_runner.cpp`는 **옮기지도 나누지도 않는다.** 근거는 설계에 있다.
* `include/re2dj/platform/linux/`의 공개 헤더 둘은 옮기지 않는다.
* 파일 이동은 내용 변경과 같은 커밋에 섞더라도 Git이 rename으로 인식하는 상태를 유지한다.

*No behavior change: only file locations and includes move, leaving function bodies, conditions, and log strings untouched. `native_create_file_observation.cpp` and `original_runner.cpp` neither move nor split, for the reason in the design. The two public headers under `include/re2dj/platform/linux/` stay. Keep the moves recognizable to Git as renames.*

## 검증 / Verification

1. Linux i386(`linux-x86-debug`) 전체 빌드와 단위 테스트.
2. Linux i386 helper(`linux-x86-helper`) 빌드. 이 구성이 옮긴 파일 다수를 쓰므로 반드시 본다.
3. Linux x64(`linux-x64-debug`) 전체 빌드와 단위 테스트. 루트에 남긴 분류가 맞는지 확인하는 검증이며, x86 전용 파일이 잘못 남아 있으면 여기서 깨진다.
4. Windows x86 빌드와 단위 테스트. Linux 이동이 공용 코드를 건드리지 않았음을 확인한다.
5. 실제 `roms/ez2dj4th/ez2dj4th.chd`로 작업 344의 `--linux-in-process-createfile-call` 회귀를 다시 돌려 facade base, export 주소, `CreateFileA("\\\\.\\NTICE")` 도달이 이동 전과 같은지 확인한다.

*Build and test Linux i386, build the Linux i386 helper configuration because it consumes many of the moved files, and build and test Linux x64 — that last one is what proves the classification, since a mistakenly retained x86-only file breaks there. Build and test Windows x86 to confirm shared code was untouched, then re-run Task 344's real 4th CHD regression and confirm the facade base, export addresses, and `CreateFileA("\\\\.\\NTICE")` match the pre-move result.*

## 완료 조건 / Completion criteria

* `src/platform/linux/` 루트에 32비트 가드 전용 source가 남지 않는다.
* 위 검증 5개가 통과하고 결과를 작업 로그에 기록한다.
* Git 이력에서 이동이 rename으로 보인다.
* 원본 CHD는 읽기 전용으로 사용한다.

*No source under a 32-bit-only guard remains at the `src/platform/linux/` root; the five verifications pass and are recorded in the work log; the moves appear as renames in history; and the original CHD is used read-only.*

## 범위 밖 / Out of scope

`src/platform/windows/` 재배치, `#if` 분리, Linux x64 compatibility-mode trampoline, 기능 추가.

*Reorganizing `src/platform/windows/`, splitting `#if` blocks, the Linux x64 compatibility-mode trampoline, and new features.*
