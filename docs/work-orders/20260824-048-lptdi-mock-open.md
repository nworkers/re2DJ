# LPTDI 가상 개방 모킹 작업 지시

관련 설계: [병렬포트 디바이스 검사 HLE 계획](../design/20260823-047-lptdi-device-hle-plan.md) — Phase B 구현 단위

## 목표

보호 스텁의 `CreateFileA("\\.\LPTDIn")` 개방이 **성공한 것처럼** 보이게 해 검사를 통과시키고, 한 번도 실행된 적 없는 성공 경로를 관찰한다. "개방 실패 → 복호화 생략 → 가비지 점프 사망" 인과([작업 로그 20260823-046](../work-logs/20260823-046-teardown-attribution.md))가 맞는지 확인하는 첫 실험이다.

## 설계 결정

* 모킹은 주입 runtime(`injected_runtime.cpp`)의 `Re2djVfsCreateFileA`에서 수행한다. 게스트 IAT는 이미 이 wrapper로 향하므로 게스트 코드 수정이 없다.
* 가상 핸들은 host 핸들과 겹치지 않는 예약 범위 `0xFEED0001..0xFEED00FF`를 쓴다.
* 포트 숫자는 실행마다 변조되므로 접두사 `\\.\lptdi`(대소문자 무시)로 판별한다.
* 모킹 정책은 runtime export 변수 `g_re2dj_device_mock`(0=자연 실패, 1=성공)으로 전달하고, launcher가 기존 VFS 설정 기록 방식(`FindPe32ExportRva` + `WriteRemoteU32`)으로 쓴다.

## 진행 상태와 다음 세션 인계 (2026-08-24 기준)

브랜치: `ez2dj-teardown-attribution`. 아래 1~2번은 커밋 완료, 3번은 WIP 커밋에 포함됨, 4번 이후 미착수.

### 완료 (커밋됨)

1. Phase A — watch list에 `DeviceIoControl`·`ReadFile`·`WriteFile`·`CloseHandle` 추가, canonical 실행에서 **개방 후 디바이스 핸들 사용 0회** 확인(개방 자체가 드라이버 존재 확인). 커밋 `c5d8399`.
2. HLE 3단계 계획 확정. 커밋 `c182807`.

### WIP (`src/platform/windows/injected_runtime.cpp`, 빌드·CTest 통과 확인)

3. 추가된 것:
   * export `volatile DWORD g_re2dj_device_mock = 0;`
   * `constexpr std::uintptr_t kDeviceMockHandleBase = 0xFEED0000;`
   * `IsDeviceMockHandle(HANDLE)` — 범위 +1..+0xFF 판별
   * `HasDeviceMockPrefix(const char*)` — `\\.\lptdi` 접두사, `g_re2dj_device_mock != 0`일 때만 참

### 남은 작업 (순서대로)

4. **injected_runtime.cpp wrapper 분기**:
   * `Re2djVfsCreateFileA`: null-name 검사 뒤 `HasDeviceMockPrefix(name)`이면 `SetLastError(ERROR_SUCCESS)` 후 `(HANDLE)(kDeviceMockHandleBase + 1)` 반환(MapVfsPath 이전에 배치).
   * `Re2djVfsReadFile`: 모킹 핸들이면 `*transferred=0`, TRUE(EOF형).
   * `Re2djVfsWriteFile`: FALSE + `ERROR_ACCESS_DENIED`.
   * `Re2djVfsSetFilePointer`: `INVALID_SET_FILE_POINTER` + `ERROR_INVALID_FUNCTION`.
   * `Re2djVfsGetFileSize`: `INVALID_FILE_SIZE` + `ERROR_INVALID_FUNCTION`.
   * `Re2djVfsCloseHandle`: TRUE(소비만).
   * `Re2djVfsGetFileType`: `FILE_TYPE_CHAR`(실제 \\.\ 디바이스 의미 유지).
   * 참고: `DeviceIoControl`은 아직 wrapper가 없어 실제 kernel32로 간다 — 가상 핸들을 받으면 실패하며, 그 호출 여부 자체가 관찰 데이터다(api watch가 잡는다).
5. **launcher main.cpp**:
   * 옵션 변수 추가: `bool device_mock_lptdi = false;`(~1961 부근 옵션 블록).
   * 파싱 추가(~2060 `--api-trace` 근처): `--device-mock-lptdi` → `device_mock_lptdi=true; inject_runtime=true; hle_vfs=true;`
   * `PrintUsage` 문자열에 `[--device-mock-lptdi]` 추가.
   * vfs 설정 블록(~2294-2331, overlay root 기록 직후): `device_mock_lptdi`면 `FindPe32ExportRva(runtime_path,"g_re2dj_device_mock",&mock_rva,&error)` + `WriteRemoteU32(child.hProcess, runtime_base+mock_rva, 1, &error)`로 `vfs_prepared`에 반영.
   * launch diagnostic(~2132)에 `device_mock` 필드 추가.
6. **빌드·CTest**: `cmake --build --preset windows-x86-debug --config Debug` 후 `ctest --test-dir build\windows-x86 -C Debug --output-on-failure`. runtime DLL 재빌드로 staged 복사본도 갱신되는지 확인(FindBundledRuntime 경로).
7. **비교 실행**(repo 루트에서):
   ```text
   --hdd roms/ez2dj1stse --hle-vfs --api-trace                          # baseline(mock off)
   --hdd roms/ez2dj1stse --hle-vfs --api-trace --device-mock-lptdi      # mock on
   ```
8. **관찰 포인트**:
   * mock on 실행에서 `\\.\LPTDI1`의 kernel32 레벨 `api_call` 기록이 **사라지면** interception 성공(IAT가 wrapper로 바뀌어 guest 호출이 kernel32에 닿지 않음).
   * 이후 (a) 스텁이 더 진행하는가 — caller 주소가 `.gtide`(`0x01ed1000..0x01ed696e`) 밖, 특히 원본 `.text`(`0x00401000..0x00453540`)인 새 api_call, 혹은 ExitProcess breakpoint까지 정상 종료가 신호. (b) 여전히 힙 페이지 #UD인가 — `fault_page_dump` 내용 비교. (c) 다른 지점 사망인가.
   * `[0x01ed7074]` 플래그는 직접 덤프하지 않으므로 필요하면 별도 관찰을 추가한다.
9. **판정과 문서화**: 성공 시 continuation page에 코드가 채워지는지(fault_page_dump 변화)로 "개방 실패 → 복호화 생략" 인과 확정. 실패 시 Winsock 프로브 등 다른 검사 후보로 Phase A식 관찰 확대. 결과를 `docs/work-logs/20260824-048-lptdi-mock-open.md`(신규)와 [구조 문서](../analysis/ez2dj-exe-structures.md) §2.5, [HDD 레이아웃](../analysis/ez2dj-hdd-layout.md), TODO에 반영하고 커밋한다.

## 해석 경계

모킹 on/off 차이가 곧 인과 증명이다. perturbation 민감성이 크므로(실행별 변주 기록 있음) 가능하면 두 구성을 각 2회 이상 실행해 변동성을 함께 기록한다.

## 검증

빌드·CTest 통과 + 두 실행 로그 비교로 위 8번 관찰 포인트를 충족하는지로 판정한다.

## 완료 상태 (2026-08-24)

4~9번을 완료했다. 비교 실행에 필요했던 기존 launcher의 runtime 주입 + debug-event 재개 결합 오류도 같은 범위에서 수정했다. mock-on은 원본 entry에 도달했으므로 작업 목표를 충족했다. 다만 이후 원본 초기화가 `0x19d521bd` 실행 access violation으로 종료되어 별도 후속 작업으로 남겼다. 결과와 해석 경계는 [작업 로그 048](../work-logs/20260824-048-lptdi-mock-open.md)에 기록했다.

---

# LPTDI Mock Open Work Order

Related design: [Parallel-Port Device Check HLE Plan](../design/20260823-047-lptdi-device-hle-plan.md) — Phase B implementation unit

## Goal

Make the protection stub's `\\.\LPTDIn` open succeed through the injected-runtime file wrappers and observe the never-executed success path — the first experiment for the inferred "failed open → skipped decryption → garbage jump" causality.

## Design decisions

Mock inside `Re2djVfsCreateFileA` (guest IAT already points there); synthetic handles in reserved range `0xFEED0001..0xFEED00FF`; prefix matching on `\\.\lptdi` case-insensitively; policy travels via runtime export `g_re2dj_device_mock` written by the launcher with the existing export-write pattern.

## Progress and next-session handoff (as of 2026-08-24)

Branch `ez2dj-teardown-attribution`; items 1–2 committed, item 3 in WIP commit, 4 onward not started:

1. Done — Phase A watch list plus zero device-handle usage finding (commit `c5d8399`); 2. done — phased HLE plan (commit `c182807`); 3. WIP in `injected_runtime.cpp`: exported `g_re2dj_device_mock`, handle-base constant, `IsDeviceMockHandle`, `HasDeviceMockPrefix` (builds; CTest green).

Remaining, in order: (4) wire wrappers — CreateFileA early-return synthetic handle before MapVfsPath, ReadFile TRUE/zero-bytes, WriteFile FALSE/access-denied, SetFilePointer/GetFileSize fail, CloseHandle consume-TRUE, GetFileType FILE_TYPE_CHAR, noting DeviceIoControl stays unwrapped so real failures are themselves observations; (5) launcher option `--device-mock-lptdi` implying inject_runtime+hle_vfs, usage text, config write via FindPe32ExportRva/WriteRemoteU32 in the vfs block (~2294–2331), launch-diagnostic field; (6) rebuild all targets including the staged runtime DLL, run CTest; (7) comparison runs `--hle-vfs --api-trace` vs `... --device-mock-lptdi`; (8) observe — missing kernel32-level record proves interception; progress signals include new api_calls with callers outside `.gtide` (game `.text` ≈ 0x00401000–0x00453540) or clean exit-breakpoint arrival, versus continued heap-page #UD (compare `fault_page_dump`); (9) judge causality, document in new work log `20260824-048-lptdi-mock-open.md` plus structures §2.5, HDD layout, TODO, and commit.

Run each configuration more than once when possible: the choreography is perturbation-sensitive across runs.

## Interpretation boundary

The mock-on/mock-off difference is the causality proof. Until it exists, keep the failed-open-to-skipped-decryption link marked as inferred.

## Verification

Build and CTest pass, plus the observation points above from the two diagnostic logs.

## Completion status (2026-08-24)

Items 4–9 are complete. The existing launcher bug combining runtime injection with debug-event resume, which blocked the comparison, was fixed in scope. Mock-on reached the original entry and therefore met this task's goal. Original initialization then terminated with an execute access violation at `0x19d521bd`, retained as separate follow-up work. Results and interpretation boundaries are recorded in [work log 048](../work-logs/20260824-048-lptdi-mock-open.md).
