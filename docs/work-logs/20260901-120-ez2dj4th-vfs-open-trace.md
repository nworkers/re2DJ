# 작업 로그 120 — ez2dj4th bounded VFS open trace

## 한국어

### 결과

injected runtime의 <code>Re2djVfsCreateFileA</code>에 bounded request/result
trace를 추가했습니다. request event는 요청 경로·access·disposition·flags를
기록하고, result event는 device·unmapped·chd·overlay·native stage와
success/error를 기록합니다. 별도 128-message budget을 사용하며 기존 파일
매핑과 I/O 의미는 변경하지 않았습니다.

실제 4th CHD trace에서 resolver 내부 event는 다음과 같이 남았습니다.

~~~text
re2dj:vfs:dynamic-resolver:name=GetVersion:route=win32
re2dj:vfs:dynamic-resolver:name=CreateFileA:route=hle
~~~

그러나 같은 VFS log에는
<code>re2dj:vfs:create-file:stage=request</code>가 없었습니다. 따라서
resolver가 HLE 함수 주소를 선택한 사실은 확인되지만,
<code>Re2djVfsCreateFileA</code>가 실제 호출되었거나 CHD pseudo-handle을
반환했다는 사실은 확인되지 않았습니다. child는 asset-open event 전에
<code>0xc0000005</code> execute fault로 종료했습니다.

### 검증

* 브랜치: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug injected-runtime build: 성공
* 실제 CHD bounded trace:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
~~~

  diagnostic log:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-104016-858.jsonl</code>
  및 VFS log를 확인했습니다. JSONL에는
  <code>vfs_dynamic_resolver(enabled=true, slots=2)</code>와
  <code>api_trace_boundary(reason=child_exit, events=33,
  code=0xc0000005)</code>가 남았습니다.
* unit tests: <code>checks: 999, failures: 0</code>
* product-loader probe:
  <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* <code>git diff --check</code>: 성공

원본 CHD와 staging 디렉터리는 저장소에 추가하지 않았습니다.

## English

### Result

The injected runtime now has bounded request/result tracing around
<code>Re2djVfsCreateFileA</code>. Request events record path, access,
disposition, and flags; result events record device, unmapped, CHD, overlay,
and native stages with success/error. A separate 128-message budget is used,
and existing path and I/O semantics are unchanged.

The resolver-internal events in the real 4th CHD trace were:

~~~text
re2dj:vfs:dynamic-resolver:name=GetVersion:route=win32
re2dj:vfs:dynamic-resolver:name=CreateFileA:route=hle
~~~

The same VFS log contained no
<code>re2dj:vfs:create-file:stage=request</code> event. Resolver selection of
an HLE function address is therefore confirmed, but an actual
<code>Re2djVfsCreateFileA</code> call or CHD pseudo-handle return is not.
The child exited with an <code>0xc0000005</code> execute fault before an
asset-open event.

### Verification

* Branch: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug injected-runtime build: passed
* Real CHD bounded trace:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
~~~

  Its diagnostic log was
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-104016-858.jsonl</code>
  and its VFS log was checked. The JSONL recorded
  <code>vfs_dynamic_resolver(enabled=true, slots=2)</code> and
  <code>api_trace_boundary(reason=child_exit, events=33,
  code=0xc0000005)</code>.
* Unit tests: <code>checks: 999, failures: 0</code>
* Product-loader probe:
  <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* <code>git diff --check</code>: passed

The original CHD and staging directory were not added to the repository.
