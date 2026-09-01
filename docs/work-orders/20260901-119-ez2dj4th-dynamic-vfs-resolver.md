# 작업 119 — ez2dj4th 동적 VFS resolver

상태: 구현 및 실제 CHD trace 검증 완료. 정상 게임 실행과 보호 응답은 미확정입니다.

## 한국어

관련 설계: [ez2dj4th 동적 VFS resolver 설계](../design/20260901-119-ez2dj4th-dynamic-vfs-resolver.md)

### 목표

작업 118에서 확인한 4th protected stub의 동적
<code>GetProcAddress("CreateFileA")</code> 결과를 4th CHD VFS wrapper로
연결합니다. 이 작업은 확인된 파일 API 경계만 추가하며, Hardlock 응답이나
게임 로직은 구현하지 않습니다.

### 구현 항목

1. <code>TargetRunDefaults::hle_dynamic_vfs</code> capability를 추가하고
   <code>ez2dj4th</code>에서만 활성화합니다.
2. injected runtime에 동적 VFS resolver enable export를 추가합니다.
3. 4th capability가 켜진 launcher 실행에서 원본
   <code>GetProcAddress</code> IAT를 runtime thunk로 patch하고 enable flag를
   기록합니다.
4. 동적 VFS capability와 기존 LPTDI device-mock capability가 서로의
   <code>DeviceIoControl</code>/WTS 경계를 암묵적으로 활성화하지 않도록
   runtime 조건을 분리합니다.
5. product-loader probe에 4th capability 검사를 추가합니다.
6. 분석 문서, <code>TODO</code>, architecture, 작업 로그에 실제 실행 결과와
   확인됨/미확정을 기록합니다.

### 제외 항목

* <code>GetVersion</code> HLE 또는 OS version semantics 추정
* <code>FindFirstFileA</code>/<code>FindNextFileA</code> directory enumeration
* Hardlock IOCTL response, seed solver, raw I/O, 보호 해제 판정
* CHD reader, FAT32 lookup, overlay write policy의 의미 변경
* 원본 EXE, CHD, HDD asset의 저장소 추가
* 1st/3rd profile의 기존 device-mock HLE 동작 변경

### 완료 조건

* 4th profile이 asset-free product-loader probe에서
  <code>hle_dynamic_vfs=true</code>로 확인됩니다.
* Windows x86 Debug build가 성공합니다.
* 실제 4th CHD 진단 로그에 dynamic resolver 준비 event가 남습니다.
* 실제 trace에서 동적 <code>CreateFileA</code> 요청이 runtime resolver를
  통과하고, 가능한 경우 VFS trace가 남습니다.
* 보호 stub의 후속 fault 또는 정상 실행 여부는 확인 상태를 과장하지 않고
  별도로 기록합니다.
* 대응 설계, 작업 로그가 존재하고 변경이 하나의 Git commit으로 남습니다.

## English

Status: implementation and real-CHD trace verification complete. Normal game
execution and the protection response remain unresolved.

Related design: [ez2dj4th dynamic VFS resolver design](../design/20260901-119-ez2dj4th-dynamic-vfs-resolver.md)

### Goal

Route the dynamic <code>GetProcAddress("CreateFileA")</code> result observed
from the 4th protected stub in task 118 through the 4th CHD VFS wrapper. This
task adds only the confirmed file-API boundary; it does not implement
Hardlock responses or game logic.

### Implementation items

1. Add <code>TargetRunDefaults::hle_dynamic_vfs</code> and enable it only for
   <code>ez2dj4th</code>.
2. Add an exported dynamic-VFS-resolver enable flag to the injected runtime.
3. For a launcher run with the 4th capability, patch the original
   <code>GetProcAddress</code> IAT to the runtime thunk and write the enable flag.
4. Keep the dynamic-VFS condition separate from the existing LPTDI device-mock
   condition so it cannot implicitly enable <code>DeviceIoControl</code> or WTS
   boundaries.
5. Add the 4th capability assertion to the product-loader probe.
6. Record the real-run result and confirmed/unresolved status in the analysis,
   <code>TODO</code>, architecture, and work log.

### Out of scope

Do not add <code>GetVersion</code> HLE or guessed OS-version semantics,
directory enumeration for <code>FindFirstFileA</code>/<code>FindNextFileA</code>,
Hardlock IOCTL responses, a seed solver, raw I/O, a protection-success
decision, CHD/FAT32/overlay semantic changes, original assets, or changes to
existing 1st/3rd device-mock behavior.

### Completion criteria

* The asset-free product-loader probe confirms
  <code>hle_dynamic_vfs=true</code> for the 4th profile.
* The Windows x86 Debug build succeeds.
* The real 4th CHD diagnostic log contains a dynamic-resolver preparation event.
* The dynamic <code>CreateFileA</code> request reaches the runtime resolver and,
  where possible, produces a VFS trace.
* Any remaining protected-stub fault or normal execution status is recorded
  without overstating its confirmation state.
* A corresponding design and work log exist, and the changes are left in one
  Git commit.
