# ez2dj4th 동적 VFS resolver 설계

## 상태

구현 완료. 4th 실제 CHD bounded trace에서 관찰된 동적
<code>GetProcAddress("CreateFileA")</code> 요청을 기존 CHD VFS 경계에 연결하는
범위로 정의했으며, resolver flag와 VFS route를 실제 trace에서 확인했습니다.

## 한국어

### 문제와 근거

작업 118의 실제 4th trace에서 protected stub은 정적 import slot을 거치지 않고
<code>GetProcAddress</code>로 <code>GetVersion</code>과
<code>CreateFileA</code>를 요청했습니다. 현재 injected runtime의
<code>Re2djHleGetProcAddress</code>는 LPTDI device mock이 켜진 경우에만 파일
API도 HLE 주소로 돌립니다. 4th profile은 CHD VFS만 사용하므로 이 조건을
충족하지 않아 동적 <code>CreateFileA</code> 결과가 native Win32 주소로
돌아갑니다.

이는 보호 계약이나 정상 실행 성공을 확인한 사실이 아닙니다. 확인된 것은 동적
resolver 요청 순서와 현재 HLE 연결 조건의 불일치입니다.

### 설계 결정

1. 플랫폼 공용 <code>TargetRunDefaults</code>에
   <code>hle_dynamic_vfs</code> capability flag를 추가합니다.
2. 실제 CHD VFS와 동적 resolver 요청이 함께 확인된
   <code>ez2dj4th</code> profile만 이 flag를 켭니다. 1st/3rd의 기존 device
   mock 동작은 별도 조건으로 유지합니다.
3. injected runtime에 export된
   <code>g_re2dj_vfs_dynamic_resolver</code>를 추가하고, launcher가 4th
   profile 실행 시에만 1을 기록합니다.
4. flag가 켜진 경우 <code>Re2djHleGetProcAddress</code>는
   <code>CreateFileA</code>, <code>ReadFile</code>, <code>WriteFile</code>,
   <code>SetFilePointer</code>, <code>GetFileSize</code>,
   <code>CloseHandle</code>, <code>GetFileType</code>을 기존 VFS wrapper로
   반환합니다. <code>DeviceIoControl</code>과 WTS observe wrapper는 기존처럼
   device mock 조건에서만 연결합니다.
5. <code>GetVersion</code>은 HLE 대상에 포함하지 않습니다. runtime의 native
   resolver 결과를 보존하여 이번 작업이 확인되지 않은 OS version semantics를
   추가하지 않도록 합니다.
6. 정적 VFS IAT patch, CHD FAT32 lookup, overlay write policy는 변경하지
   않습니다. <code>FindFirstFileA</code>/<code>FindNextFileA</code> 구현은
   별도 작업으로 남깁니다.

~~~mermaid
flowchart TD
    A[ez2dj4th TargetRunDefaults] --> B[hle_vfs]
    A --> C[hle_dynamic_vfs]
    C --> D[launcher writes runtime resolver flag]
    D --> E[patched GetProcAddress IAT]
    E --> F[Re2djHleGetProcAddress]
    F -->|file API names| G[CHD VFS wrappers]
    F -->|GetVersion or unrelated name| H[native Win32 resolver]
    I[1st/3rd device mock] --> J[existing device and WTS dynamic routes]
~~~

### 파일별 변경 경계

* <code>include/re2dj/target/target_profile.h</code>와
  <code>src/target/target_profile.cpp</code>: capability field와 4th profile
  선언을 추가합니다.
* <code>src/platform/windows/injected_runtime.cpp</code>: resolver capability
  export와 조건을 분리합니다.
* <code>src/tools/windows_x86_launcher_probe/main.cpp</code>: 4th capability가
  있으면 <code>GetProcAddress</code> IAT를 patch하고 runtime flag를 기록합니다.
  기존 device mock 진단 event와 설정은 보존합니다.
* <code>src/tools/windows_product_loader_probe/main.cpp</code>: asset-free
  profile assertion으로 4th capability 전달을 확인합니다.
* <code>docs/analysis/</code>, <code>docs/TODO.md</code>,
  <code>ARCHITECTURE.md</code>, work-order/work-log: 확인된 동적 resolver
  경계와 미확정 결과를 갱신합니다.

### 검증 결과

* Windows x86 Debug launcher와 product-loader probe를 빌드합니다.
* product-loader probe가 4th profile의 <code>hle_dynamic_vfs</code>를
  확인하는지 검사합니다.
* 실제 CHD trace에서
  <code>vfs_dynamic_resolver(enabled=true, slots=2)</code>가 기록되었습니다.
* VFS log에서 <code>GetVersion:route=win32</code>에 이어
  <code>CreateFileA:route=hle</code>가 기록되었습니다.
* asset-open event 전에 child가 <code>0xc0000005</code>로 종료되었으며,
  보호 stub의 후속 실행은 미확정입니다.

## English

### Status

Implementation complete. The scope was to route the dynamic
<code>GetProcAddress("CreateFileA")</code> request observed in the real 4th CHD
bounded trace through the existing CHD VFS boundary, and the resolver flag plus
VFS route were confirmed in the real trace.

### Problem and evidence

The real 4th trace from task 118 showed the protected stub requesting
<code>GetVersion</code> and <code>CreateFileA</code> through
<code>GetProcAddress</code>, without using a static import slot. The injected
runtime currently routes file APIs to HLE addresses from
<code>Re2djHleGetProcAddress</code> only when the LPTDI device mock is enabled.
The 4th profile uses CHD VFS without that device mock, so the dynamically
resolved <code>CreateFileA</code> returns a native Win32 address.

This does not confirm the protection contract or successful execution. It
confirms the dynamic resolver order and the mismatch with the current HLE
connection condition.

### Design decisions

1. Add the platform-neutral <code>hle_dynamic_vfs</code> capability flag to
   <code>TargetRunDefaults</code>.
2. Enable it only for the <code>ez2dj4th</code> profile, where real CHD VFS and
   dynamic resolver requests are both confirmed. Preserve existing 1st/3rd
   device-mock behavior under its separate condition.
3. Add the exported
   <code>g_re2dj_vfs_dynamic_resolver</code> flag and have the launcher write 1
   only for a 4th-profile execution.
4. When enabled, <code>Re2djHleGetProcAddress</code> returns the existing VFS
   wrappers for <code>CreateFileA</code>, <code>ReadFile</code>,
   <code>WriteFile</code>, <code>SetFilePointer</code>,
   <code>GetFileSize</code>, <code>CloseHandle</code>, and
   <code>GetFileType</code>. <code>DeviceIoControl</code> and the WTS observe
   wrapper remain gated by the existing device-mock condition.
5. Do not HLE <code>GetVersion</code>. Preserve the native resolver result so
   this task does not add unverified OS-version semantics.
6. Do not change static VFS IAT patching, CHD FAT32 lookup, or overlay write
   policy. <code>FindFirstFileA</code>/<code>FindNextFileA</code> remain a
   separate task.

### File boundaries

* <code>include/re2dj/target/target_profile.h</code> and
  <code>src/target/target_profile.cpp</code>: add the capability field and the
  4th profile declaration.
* <code>src/platform/windows/injected_runtime.cpp</code>: add the resolver
  capability export and separate its condition.
* <code>src/tools/windows_x86_launcher_probe/main.cpp</code>: patch the
  <code>GetProcAddress</code> IAT and write the runtime flag when the 4th
  capability is present. Preserve existing device-mock diagnostics and setup.
* <code>src/tools/windows_product_loader_probe/main.cpp</code>: assert 4th
  capability propagation in the asset-free profile test.
* Update <code>docs/analysis/</code>, <code>docs/TODO.md</code>,
  <code>ARCHITECTURE.md</code>, and the work-order/work-log with the confirmed
  dynamic-resolver boundary and unresolved result.

### Verification result

* Build the Windows x86 Debug launcher and product-loader probe.
* Verify that the product-loader probe checks the 4th profile's
  <code>hle_dynamic_vfs</code> capability.
* The real CHD trace recorded
  <code>vfs_dynamic_resolver(enabled=true, slots=2)</code>.
* The VFS log recorded
  <code>GetVersion:route=win32</code> followed by
  <code>CreateFileA:route=hle</code>.
* No asset-open event was observed before the child exited with
  <code>0xc0000005</code>; the protected-stub continuation remains unresolved.
