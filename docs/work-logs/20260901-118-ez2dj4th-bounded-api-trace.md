# 작업 로그 118 — ez2dj4th 보호 stub bounded API trace

## 한국어

### 결과

정적 <code>ExitProcess</code> import가 없는 4th 실행 파일에 대해
<code>--api-trace</code>가 준비 단계에서 실패하지 않고 bounded event pump으로
entry 이후 Win32 API breakpoint를 관찰하도록 launcher를 수정했습니다. child가
종료되거나 event cap에 도달하면 <code>api_trace_boundary</code>를 기록합니다.
이 변경은 보호 응답이나 게임 로직을 추가하지 않습니다.

실제 CHD trace에서 첫 동적 resolver 대상은 다음 순서로 확인되었습니다.

1. caller <code>0x00af0b99</code>가 <code>GetVersion</code>을 요청했습니다.
2. caller <code>0x00af09f6</code>가 <code>CreateFileA</code>를 요청했습니다.
3. child가 <code>0xc0000005</code>로 종료되었고
   <code>api_trace_boundary</code>가 <code>reason=child_exit</code>,
   <code>events=11</code>로 기록되었습니다.

따라서 현재 결과는 동적 <code>CreateFileA</code> 결과를 4th CHD VFS wrapper로
연결하기 전의 관찰 증거입니다. trace가 debug breakpoint와 single-step 상태를
변경하므로 fault의 직접 원인과 정상 <code>CreateFileA</code> 반환 여부는
미확정입니다.

### 검증

* 현재 브랜치: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug launcher build:

~~~text
MSBuild build\windows-x86\re2dj_windows_x86_launcher_probe.vcxproj /p:Configuration=Debug /m /v:minimal
~~~

  성공했습니다. 로컬 환경에서 <code>cmake</code> 명령은 PATH에 없어서 기존
  생성된 Visual Studio project와 절대 경로의 MSBuild를 사용했습니다.
* <code>re2dj.exe ez2dj4th --list-targets</code>: 성공했습니다.
* 실제 CHD 일반 실행:

~~~text
build\windows-x86\bin\Debug\re2dj.exe ez2dj4th --run
~~~

  exit code 1로 bounded timeout에 도달했습니다. 진단 로그는
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-095633-849.jsonl</code>이며,
  VFS mount와 image-loader 준비는 기록되었지만 VFS <code>CreateFileA</code> event는
  기록되지 않았습니다.
* 실제 CHD bounded trace는 다음 명령으로 수행했습니다.

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --api-trace --trace
~~~

  exit code 0으로 진단 경계까지 도달했습니다. 진단 로그는
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-100606-105.jsonl</code>입니다.
* 변경 파일에 대해 <code>git diff --check</code>를 수행할 예정이며, 기존
  product-loader probe도 커밋 전 재확인합니다.

원본 CHD와 staging 디렉터리는 저장소에 추가하지 않았습니다.

## English

### Result

The launcher now lets <code>--api-trace</code> observe post-entry Win32 API
breakpoints for the 4th executable even when its static imports have no
<code>ExitProcess</code>. It uses a bounded event pump and records
<code>api_trace_boundary</code> when the child exits or the event cap is reached.
This change adds no protection response or game logic.

The real CHD trace confirmed the first dynamic resolver targets in this order:

1. caller <code>0x00af0b99</code> requested <code>GetVersion</code>;
2. caller <code>0x00af09f6</code> requested <code>CreateFileA</code>;
3. the child exited with <code>0xc0000005</code>, and
   <code>api_trace_boundary</code> recorded <code>reason=child_exit</code> with
   <code>events=11</code>.

This is observation evidence before routing the dynamic <code>CreateFileA</code>
result through the 4th CHD VFS wrapper. Because tracing changes debug
breakpoint and single-step state, the direct cause of the fault and whether a
normal <code>CreateFileA</code> return occurred remain unresolved.

### Verification

* Branch: <code>task-113-ez2dj4th-chd</code>
* The Windows x86 Debug launcher build succeeded:

~~~text
MSBuild build\windows-x86\re2dj_windows_x86_launcher_probe.vcxproj /p:Configuration=Debug /m /v:minimal
~~~

  <code>cmake</code> was not on PATH in the local environment, so the existing
  Visual Studio project and an absolute MSBuild path were used.
* <code>re2dj.exe ez2dj4th --list-targets</code> passed.
* The real-CHD normal run:

~~~text
build\windows-x86\bin\Debug\re2dj.exe ez2dj4th --run
~~~

  reached the bounded timeout with exit code 1. Its diagnostic log was
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-095633-849.jsonl</code>.
  It recorded VFS mount and image-loader preparation but no VFS
  <code>CreateFileA</code> event.
* The real-CHD bounded trace used:

~~~text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --api-trace --trace
~~~

  It reached the diagnostic boundary with exit code 0. Its diagnostic log was
  <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-100606-105.jsonl</code>.
* <code>git diff --check</code> and the existing product-loader probe are
  rechecked before commit.

The original CHD and staging directory were not added to the repository.
