# 작업 로그 122 — ez2dj4th resolver caller instruction window

## 한국어

### 결과

resolver가 캡처한 원본 caller return address 주변의 child runtime memory를
앞 8바이트·뒤 16바이트, 총 24바이트로 읽어 VFS log에 기록했습니다.
<code>CreateFileA</code> caller window은 다음과 같습니다.

~~~text
base=0x00af09ee
caller=0x00af09f6
readable=1
bytes=7f50ff15a80caf000f878945dc66c1e2200f8103f4ffff70044c
~~~

caller offset 8에서 시작하는 <code>89 45 dc</code>는
<code>MOV [EBP-0x24], EAX</code>로 해석되며, resolver 반환값이 즉시
<code>Re2djVfsCreateFileA</code>를 호출하는 대신 protected stack-local에
저장되는 경계가 **확인됨**입니다. <code>GetVersion</code> caller window도
readable이었으며 반환 직후 conditional branch bytes가 기록되었습니다.

저장된 pointer의 후속 consumer와 indirect call은 관찰되지 않았습니다.
child는 여전히 <code>eip=0x00000000</code> execute fault로 종료했으므로
ABI 호환, 첫 파일 open, 보호 응답과 정상 실행은 **미확정**입니다.

### 검증

* 브랜치: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug injected-runtime build: 성공
* 실제 CHD bounded trace:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
~~~

  VFS log:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-105357-873.vfs.log</code>
* unit tests: <code>checks: 999, failures: 0</code>
* product-loader probe:
  <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* <code>git diff --check</code>: 성공

caller window은 실행 중 memory에서 읽었으며 원본 EXE·CHD·HDD asset은
저장소에 추가하지 않았습니다.

## English

### Result

The runtime now reads and records a 24-byte child-memory window consisting of
8 bytes before and 16 bytes after the resolver-captured original caller return
address. The <code>CreateFileA</code> caller window was:

~~~text
base=0x00af09ee
caller=0x00af09f6
readable=1
bytes=7f50ff15a80caf000f878945dc66c1e2200f8103f4ffff70044c
~~~

The <code>89 45 dc</code> bytes at caller offset 8 decode as
<code>MOV [EBP-0x24], EAX</code>. This confirms a boundary where the resolver
result is stored in a protected stack-local rather than an immediately
observed call to <code>Re2djVfsCreateFileA</code>. The <code>GetVersion</code>
caller window was also readable and recorded conditional-branch bytes after
the resolver return.

No later consumer or indirect call of the stored pointer was observed. The
child still exited with an execute fault at <code>eip=0x00000000</code>, so ABI
compatibility, the first file open, the protection response, and normal
execution remain **unresolved**.

### Verification

* Branch: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug injected-runtime build: passed
* Real CHD bounded trace:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
~~~

  VFS log:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-105357-873.vfs.log</code>
* Unit tests: <code>checks: 999, failures: 0</code>
* Product-loader probe:
  <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* <code>git diff --check</code>: passed

The caller window was read from live child memory. The original executable,
CHD, and HDD assets were not added to the repository.
