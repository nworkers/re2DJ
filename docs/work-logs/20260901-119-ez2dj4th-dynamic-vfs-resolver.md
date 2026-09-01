# 작업 로그 119 — ez2dj4th 동적 VFS resolver

## 한국어

### 결과

4th profile에 <code>hle_dynamic_vfs</code> capability를 추가하고, 실제 CHD
실행에서 원본 <code>GetProcAddress</code> IAT 2개를 injected runtime thunk로
연결했습니다. runtime은 4th capability가 켜진 경우에만 동적 파일 API를
기존 VFS wrapper로 반환하며, <code>DeviceIoControl</code>과 WTS observe
경계는 기존 LPTDI device mock 조건을 유지합니다.

실제 CHD bounded trace는 다음을 기록했습니다.

* diagnostic JSONL:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-102648-821.jsonl</code>
* resolver 준비:
  <code>vfs_dynamic_resolver(enabled=true, slots=2)</code>
* VFS trace:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-102648-821.vfs.log</code>
* resolver route: <code>GetVersion:route=win32</code> 다음
  <code>CreateFileA:route=hle</code>
* child 결과: asset-open event 전에 <code>0xc0000005</code> execute fault,
  bounded boundary <code>events=33</code>

동적 <code>CreateFileA</code> 결과가 HLE wrapper로 연결되는 것은
**확인됨**입니다. 그러나 실제 게임 파일 asset-open, 보호 응답, 정상 실행은
**미확정**입니다. launcher의 <code>outcome=status=success</code>는 bounded
관찰 경계 도달만 의미합니다.

### 검증

* 브랜치: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug injected runtime build: 성공
* Windows x86 Debug launcher build: 성공
* Windows x86 Debug product-loader build: 성공
* product-loader 실행:

~~~text
windows-product-loader-probe: profile-defaults=ok unsupported-target=ok
~~~

* 실제 CHD profile shortcut:

~~~text
build\windows-x86\bin\Debug\re2dj.exe ez2dj4th --run
~~~

  exit code 1로 기존 handoff timeout에 도달했습니다. 이 결과는
  <code>vfs_dynamic_resolver</code> 준비 event가 없는 normal run의
  handoff 경계 미확정 상태이며, trace 검증은 아래 launcher 명령으로
  별도 수행했습니다.
* 실제 CHD bounded trace:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
~~~

  exit code 0으로 bounded diagnostic boundary까지 도달했으며 위 resolver
  event와 VFS route를 확인했습니다.
* <code>git diff --check</code>: 성공

검증 중 launcher와 product-loader를 동시에 빌드할 때 공용 PDB
<code>re2dj_core.pdb</code>에 대한 일시적인 MSVC <code>C1041</code>이
발생했지만, product-loader를 <code>/m:1</code>로 단독 재빌드하여 성공을
확인했습니다. 원본 CHD와 staging 디렉터리는 저장소에 추가하지 않았습니다.

## English

### Result

The 4th profile now has an <code>hle_dynamic_vfs</code> capability, and the
real CHD run patched two original <code>GetProcAddress</code> IAT slots to the
injected runtime thunk. With the 4th capability enabled, the runtime returns
the existing VFS wrappers for dynamic file APIs. The
<code>DeviceIoControl</code> and WTS-observe boundaries remain gated by the
existing LPTDI device-mock condition.

The real CHD bounded trace recorded:

* diagnostic JSONL:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-102648-821.jsonl</code>
* resolver preparation:
  <code>vfs_dynamic_resolver(enabled=true, slots=2)</code>
* VFS trace:
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-102648-821.vfs.log</code>
* resolver route: <code>GetVersion:route=win32</code>, followed by
  <code>CreateFileA:route=hle</code>
* child result: an <code>0xc0000005</code> execute fault before an
  asset-open event, with a bounded boundary at <code>events=33</code>

Routing the dynamic <code>CreateFileA</code> result through the HLE wrapper is
**confirmed**. The first real game-file asset open, protection response, and
normal execution remain **unresolved**. The launcher's
<code>outcome=status=success</code> means only that the bounded observation
boundary was reached.

### Verification

* Branch: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug injected-runtime build: passed
* Windows x86 Debug launcher build: passed
* Windows x86 Debug product-loader build: passed
* Product-loader execution:

~~~text
windows-product-loader-probe: profile-defaults=ok unsupported-target=ok
~~~

* Real CHD profile shortcut:

~~~text
build\windows-x86\bin\Debug\re2dj.exe ez2dj4th --run
~~~

  reached the existing handoff timeout with exit code 1. This is the
  unresolved normal-run handoff boundary; the
  <code>vfs_dynamic_resolver</code> preparation and route were verified with
  the separate launcher command below.
* Real CHD bounded trace:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
~~~

  reached the bounded diagnostic boundary with exit code 0 and confirmed the
  resolver event plus VFS route above.
* <code>git diff --check</code>: passed

Running the launcher and product-loader builds concurrently caused a transient
MSVC <code>C1041</code> on the shared <code>re2dj_core.pdb</code>; rebuilding
the product-loader alone with <code>/m:1</code> passed. The original CHD and
staging directory were not added to the repository.
