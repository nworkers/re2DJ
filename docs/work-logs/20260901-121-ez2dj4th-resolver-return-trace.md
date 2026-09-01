# 작업 로그 121 — ez2dj4th 동적 resolver 반환 ABI trace

## 한국어

### 결과

동적 resolver VFS log에 반환 함수 포인터 주소와 resolver 진입 caller 주소를
추가했습니다. 기존 HLE/native 반환값과 calling convention은 변경하지
않았습니다.

실제 4th CHD 실행에서 다음을 확인했습니다.

~~~text
GetVersion:route=win32:address=0x77451c10:caller=0x00af0b99
CreateFileA:route=hle:address=0x62f5350d:caller=0x00af09f6
~~~

launcher diagnostic의 kernel32 base는 <code>0x77430000</code>, runtime base는
<code>0x62f50000</code>였으므로 반환 주소가 각 module 범위에 있는 것은
**확인됨**입니다. caller도 작업 118의 native API trace에서 관찰한 원본
caller와 일치합니다.

그러나 <code>Re2djVfsCreateFileA</code> request event는 계속 없었고 child는
<code>eip=0x00000000</code> execute fault로 종료했습니다. 따라서 반환
pointer 실제 호출, 정확한 ABI 호환, 첫 파일 open과 보호 응답은
**미확정**입니다.

### 검증

* 브랜치: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug injected-runtime build: 성공
* 실제 CHD bounded trace:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
~~~

  diagnostic log:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-104740-222.jsonl</code>
  및 VFS log:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-104740-222.vfs.log</code>
* unit tests: <code>checks: 999, failures: 0</code>
* product-loader probe:
  <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* <code>git diff --check</code>: 성공

원본 CHD와 staging 디렉터리는 저장소에 추가하지 않았습니다.

## English

### Result

The dynamic-resolver VFS log now records the returned function-pointer address
and the caller entering the resolver. Existing HLE/native results and calling
conventions were not changed.

The real 4th CHD run confirmed:

~~~text
GetVersion:route=win32:address=0x77451c10:caller=0x00af0b99
CreateFileA:route=hle:address=0x62f5350d:caller=0x00af09f6
~~~

The launcher diagnostic reported kernel32 base
<code>0x77430000</code> and runtime base <code>0x62f50000</code>, so both return
addresses lie within their expected module ranges. The caller also matches the
original caller seen in task 118's native API trace.

The <code>Re2djVfsCreateFileA</code> request event nevertheless remained absent,
and the child exited with an execute fault at
<code>eip=0x00000000</code>. Actual pointer invocation, exact ABI compatibility,
the first file open, and the protection response remain **unresolved**.

### Verification

* Branch: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug injected-runtime build: passed
* Real CHD bounded trace:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
~~~

  Diagnostic log:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-104740-222.jsonl</code>
  VFS log:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-104740-222.vfs.log</code>
* Unit tests: <code>checks: 999, failures: 0</code>
* Product-loader probe:
  <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* <code>git diff --check</code>: passed

The original CHD and staging directory were not added to the repository.
