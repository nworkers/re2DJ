# 작업 452 작업 로그 — Windows x64 호환 모드 조사 / Task 452 work log — probing compatibility mode on Windows x64

설계: [20261004-452-windows-x64-compat-mode-probe.md](../design/20261004-452-windows-x64-compat-mode-probe.md) · 지시서: [20261004-452-windows-x64-compat-mode-probe.md](../work-orders/20261004-452-windows-x64-compat-mode-probe.md) · 정리: [Windows x64 compatibility mode](../kb/windows-x64-compatibility-mode.md)

## 2026-10-04

- **만든 것**: `src/platform/windows/x64/native_compat_mode_probe.cpp`와 x64 전용 CMake 대상 `re2dj_windows_x64_compat_mode_probe`(`/CETCOMPAT:NO`, 제품·CTest 밖). 전환 코드는 WSL `as`로 어셈블해 바이트로 옮겼고, 0x10000000 고정 페이지에 복사한다. 질문마다 자식 프로세스로 돌려 한 질문의 충돌이 다른 결과를 가리지 않게 했다. `src/platform/windows/x64/README.md`, Windows 플랫폼 README, kb 새 문서와 색인, 오래된 kb `x86-32-guest-on-64-bit-host.md`의 Windows 행(주입 → WOW64 안 in-process).
- **도중에 고친 probe 오류**: (1) 파이프 출력이 버퍼에 남아 충돌한 질문의 출력이 사라짐 → 버퍼 끔. (2) 재개 stub(0x55바이트)을 0x1c0에 두어 0x200의 32비트 stub과 겹침 → 0x300으로. (3) 일부러 낸 null 역참조도 FS 복구로 오인해 무한 재시도 → FS 접두사(`0x64`) 확인 추가. 이 경험은 실제 backend에도 같은 확인이 필요하다는 근거다.
- **환경**: Windows 11 25H2(빌드 26200), Intel Core Ultra 5 225H(14 논리 코어), VS 2026 Build Tools 18.10 MSVC x64.

  *Made: the probe and its x64-only CMake target `re2dj_windows_x64_compat_mode_probe` (`/CETCOMPAT:NO`, outside the product and CTest); the transition code was assembled with WSL `as` and copied as bytes to a fixed page at 0x10000000; each question runs in a child process so one crash hides no other result. Also the x64 README, the Windows platform README, the new kb topic with its index entry, and the stale Windows row of `x86-32-guest-on-64-bit-host.md` (injection → in-process under WOW64). Probe bugs fixed on the way: piped output stayed buffered and vanished with a crashing question (buffering off); the 0x55-byte resume stub at 0x1c0 overlapped the 32-bit stub at 0x200 (moved to 0x300); a deliberate null dereference was taken for an FS repair and retried forever (an FS-prefix `0x64` check added), which is itself evidence a real backend needs that check. Environment: Windows 11 25H2 (build 26200), Intel Core Ultra 5 225H (14 logical cores), VS 2026 Build Tools 18.10 MSVC x64.*

## 결과 / Results

| # | 결과 / Result | 상태 / Status |
| --- | --- | --- |
| Q1 | `retfq`로 CS `0x23` 진입, 32비트 코드 실행, `jmp far 0x33`으로 복귀: 통과 | 확인됨 |
| Q2 | 기본 FS(`0x53`)의 base는 0. 호환 모드의 `fs:[0x18]`·`fs:[0]`은 접근 위반 | 확인됨 |
| Q3 | 사용자 모드 `rdfsbase`/`wrfsbase` 허용(원래 FS base 0) | 확인됨 |
| Q4 | `wrfsbase` 값은 문맥 전환 때 0으로 돌아감. `Sleep(25)` 뒤 20/20, 부하 속 3초 회전에서 297~1045회 손실(첫 손실 0.4~1 ms). 게스트 30억 회 읽기에서 VEH 지연 복구 299~2675회, 잘못 읽은 값 0회 | 확인됨 |
| Q5 | 호환 모드의 `int3`(`0x4000001F`로 옴)·접근 위반이 VEH에 `SegCs = 0x23`로 전달. `CONTEXT`로는 CS `0x33`으로 재개되어 실패하고, 64비트 stub의 `retfq`로는 재개 성공. 예외 왕복만으로는 FS base가 사라지지 않음 | 확인됨 |
| Q5 대조 | TEB 스택 한계 밖의 게스트 스택에서도 전달 자체는 됨. 그 스택에서 `printf`를 부른 초기 handler는 죽었고 `__chkstk` 때문으로 봄 | 전달 확인됨, 원인 추정 |
| Q6 | selector `0x53`을 다시 적재해도 FS base 0 | 확인됨 |

*Q1 passes: CS `0x23` entered with `retfq`, 32-bit code run, back through `jmp far 0x33` (confirmed). Q2: the default FS (`0x53`) has base 0, so compatibility-mode `fs:[0x18]` and `fs:[0]` fault (confirmed). Q3: user-mode `rdfsbase`/`wrfsbase` are allowed, the original FS base being 0 (confirmed). Q4: a `wrfsbase` value returns to 0 on a context switch, lost 20 of 20 times after `Sleep(25)` and 297 to 1045 times in 3 seconds of spinning under load (first loss after 0.4 to 1 ms); over 3 billion guest reads the VEH's lazy repair ran 299 to 2675 times with no wrong value read (confirmed). Q5: compatibility-mode `int3` (arriving as `0x4000001F`) and access violations reach the VEH with `SegCs = 0x23`; resuming through `CONTEXT` lands in CS `0x33` and fails, while a 64-bit stub's `retfq` resumes correctly; an exception round trip alone did not lose the FS base (confirmed). Q5 control: delivery works even with the guest stack outside the TEB's stack limits; the early handler that called `printf` there died, put down to `__chkstk` (delivery confirmed, cause inferred). Q6: reloading selector `0x53` still gives FS base 0 (confirmed).*

전체 실행 기록(마지막 실행, x86 빌드와 동시에 돌아 Q4가 24.8초 걸림):

```text
Q1 PASS: 32-bit code ran in CS 0x23 and returned to CS 0x33
Q2 INFO: default fs in compatibility mode does not reach a TEB
Q3 PASS: user-mode rdfsbase/wrfsbase
  host: fs base lost across Sleep(25) in 20 of 20 tries
  host: 3s of spinning with 28 load threads: 1045 losses, first after 0.0006s
  guest: 3000000000 reads in 24.77s under load, repairs=2675, other faults=0, mismatches=0, last fs:[0x18]=0x10001000
Q4 FAIL: a wrfsbase TEB does NOT hold across preemption; with VEH repair the guest always read the TEB
Q5 PASS: compatibility-mode exceptions reach the VEH and resume through a 64-bit stub
Q6 INFO: selector 0x53 gives fs base 0x0
```

## 규모 판단 / Sizing

설계의 기준으로는 둘째 줄이다. 같은 프로세스 직접 실행은 **가능**하지만, FS를 지키는 추가 장치가 필요하다. Linux x64 backend(전환·import bridge·bootstrap·low memory, 약 1,860줄)를 옮기는 것에 더해 다음이 Windows x64에만 필요하다.

1. **FS 지연 복구**: VEH가 null 영역 접근 위반 중 FS 접두사 명령만 골라 TEB를 다시 쓰고 재시도한다. 모든 게스트 진입·재개 지점에서도 `wrfsbase`를 한다.
2. **재개 stub**: 모든 VEH 재개(게스트 SEH 배달, legacy I/O 트랩, FS 복구, import 경계)는 64비트 stub을 거친다. single-step(TF)까지 되돌리려면 `iretq` 기반이어야 할 것이다(**추정**).
3. **TEB 스택 한계 교체**: 게스트가 도는 동안 64비트 TEB의 `StackBase`/`StackLimit`를 게스트 스택으로 바꾸고, 돌아오면 되돌린다. 게스트 스레드마다 같다.
4. **전환 코드**: MSVC x64에는 inline asm이 없으므로 MASM(`ml64`) 파일이나 바이트 blob으로 둔다.
5. **빌드·배포**: x64 preset, SDL3 x64, x64 in-process probe(x86의 약 600줄에 해당), CTest, 릴리스 workflow·패키지.

예상 규모는 **약 2,500~3,000줄, 작업 4개 정도**(backend와 probe / CLI·재실행·빌드 / 실게임 5종 확인 / 릴리스·문서)로 작업 446~450의 Windows 쪽(448~449)과 비슷하거나 조금 크다.

*By the design's criteria this is the second row: same-process direct execution is **possible**, but needs extra machinery guarding FS. Beyond porting the Linux x64 backend's structure (transitions, import bridge, bootstrap, low memory, about 1,860 lines), Windows x64 alone needs: lazy FS repair, the VEH picking out FS-prefixed instructions among null-region access violations to rewrite the TEB and retry, plus `wrfsbase` at every guest entry and resumption; a resume stub through which every VEH resumption passes (guest SEH delivery, legacy I/O traps, FS repair, the import boundary), probably `iretq`-based to restore single-stepping (TF) too (**inferred**); swapping the 64-bit TEB's `StackBase`/`StackLimit` to the guest stack while it runs, per guest thread; transition code as MASM (`ml64`) or byte blobs, MSVC x64 having no inline asm; and build and release work (an x64 preset, SDL3 x64, an x64 in-process probe of about the x86 one's 600 lines, CTest, the release workflow and package). The estimate is **about 2,500 to 3,000 lines over roughly four tasks** (backend and probe; CLI, relaunch and build; the five real games; release and documents), similar to or a little larger than the Windows side of tasks 446 to 450 (448 and 449).*

**위험 / Risks**

- 커널이 CS를 `0x33`으로 바꾸는 것과 FS base를 0으로 되돌리는 것은 문서화되지 않은 동작이다. 앞으로 Windows 빌드가 바뀌면 달라질 수 있다(**미확정**). 이 PC의 한 빌드에서만 확인했다.
- `re2dj.exe`가 하드웨어 shadow stack(`/CETCOMPAT`)을 계속 꺼 두어야 할 가능성이 크다(**추정**).
- ARM64 Windows의 x64 에뮬레이션이 호환 모드 전환을 지원하는지는 모른다(**미확정**). 지금의 x86 빌드는 그곳에서도 x86 에뮬레이션으로 돈다.
- FS 복구 비용: 부하 속 초당 약 57~108회로 무시할 만하다. 그러나 게임마다 실제로 확인해야 한다.

*The kernel switching CS to `0x33` and returning the FS base to 0 are undocumented behaviour that may change with future Windows builds (**unresolved**; seen on one build of this PC). `re2dj.exe` would likely have to keep hardware shadow stacks (`/CETCOMPAT`) off (**inferred**). Whether x64 emulation on ARM64 Windows supports compatibility-mode transitions is unknown (**unresolved**), whereas the current x86 build runs there under x86 emulation. FS repair cost, about 57 to 108 a second under load, is negligible but needs checking per game.*

**권고 / Recommendation:** 지금의 x86 `re2dj.exe`는 모든 64비트 Windows에서 WOW64로 돌고 위 위험이 없다. 그래서 x64 Windows backend는 **구체적인 필요**가 생길 때 시작하기를 권한다. 예를 들어 WOW64 지원 축소, 64비트 전용 의존성, 4 GiB host 주소 한계 문제가 그런 경우다. 시작한다면 첫 작업은 이 probe를 x64 in-process probe로 키워 SEH 배달과 게스트 스레드까지 확인하는 것이다.

*The current x86 `re2dj.exe` runs on every 64-bit Windows under WOW64 without these risks, so a Windows x64 backend is best started when a **concrete need** appears, such as WOW64 support shrinking, a 64-bit-only dependency, or the host's 4 GiB address limit becoming a problem; if started, the first task grows this probe into an x64 in-process probe covering SEH delivery and guest threads.*

## 검증 / Verification

- Windows x64 Debug(MSVC, `RE2DJ_WARNINGS_AS_ERRORS=ON`): probe 대상 빌드 성공, 경고 없음. 전체 probe 실행 결과는 위와 같다. Q4의 "FAIL"은 조사 결과(FS가 유지되지 않음)이지 probe 오류가 아니다.
- Windows x86 Debug(`scripts/build.ps1`, 다시 configure): CMake 변경 뒤에도 build 성공.
- x64 configure 전체(`-A x64`)는 성공했다. 제품 대상(`re2dj`)은 x64 backend가 없어 빌드하지 않았다.

*Windows x64 Debug (MSVC, `RE2DJ_WARNINGS_AS_ERRORS=ON`) builds the probe target without warnings, with the full run as above; Q4's "FAIL" is the finding (FS does not hold), not a probe error. Windows x86 Debug (`scripts/build.ps1`, reconfigured) still builds after the CMake change. The full x64 configure (`-A x64`) succeeds; the product target (`re2dj`) was not built for x64, having no x64 backend.*
