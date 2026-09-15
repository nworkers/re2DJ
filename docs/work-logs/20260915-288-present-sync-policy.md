# 작업 288 작업 로그 — present 동기화 정책 / Task 288 work log — Present synchronization policy

설계: [20260915-288-present-sync-policy.md](../design/20260915-288-present-sync-policy.md)
작업 지시: [20260915-288-present-sync-policy.md](../work-orders/20260915-288-present-sync-policy.md)

## 한국어

### 배경

`ez2dj3rd`가 56~57 fps로 동작한다는 사용자 보고에서 시작했다. 원인 조사 중 저장소가 `SDL_GL_SetSwapInterval`을 **한 번도 호출하지 않는다**는 사실을 발견했다. 즉 vsync 동작이 의도가 아니라 드라이버 기본값이었다. 이 작업은 그 정책을 명시적으로 만든다.

### 구현

| 계층 | 변경 |
| --- | --- |
| 공용 코어 | `include/re2dj/graphics/present_sync.h`에 `PresentSync`와 `PresentSyncName`/`ParsePresentSyncName`. `src/graphics/present_sync.cpp` |
| 백엔드 | `Sdl3OpenGlWindowConfig::present_sync`, `Initialize`의 interval 설정, `applied_swap_interval()` 조회 |
| Windows 전역 | `g_re2dj_present_sync` export와 `SelectedPresentSync()` |
| facade | window config에 정책 전달, 적용값을 graphics trace에 한 줄 |
| profile | `TargetRunDefaults::present_sync`(기본 vsync) |
| 인자 생성 | 비기본값일 때만 `--present-sync` 추가 |
| launcher | `--present-sync` 파싱, 전역 기록, 자식 프로세스 전달, launch 진단에 값 기록 |
| 제품 CLI | `--vsync on\|off\|adaptive`, 사용법, `_explicit` 덮어쓰기 |
| 테스트 | `tests/unit/present_sync_test.cpp` |

### 설계에서 조정한 것

설계는 백엔드가 적용값을 **직접 trace에 기록**한다고 적었다. 구현에서는 그렇게 하지 않았다. 공용 코어에는 로그 수단이 없고, Windows 전용 `graphics_trace_log`를 코어가 부르면 플랫폼 중립성이 깨진다. 대신 백엔드가 `applied_swap_interval()`로 값을 노출하고 호스트(facade)가 기록한다. 설계 문서의 해당 절은 이 결정을 반영해 고쳤다.

`PresentSync`도 설계에서는 `sdl3_opengl_backend.h`에 두기로 했으나, 그러면 `target_profile.h`가 그래픽 백엔드 인터페이스 전체에 의존하게 된다. 별도 `present_sync.h`로 분리했다.

### 검증

* Windows x86 **Debug 전체 빌드**: 컴파일·링크 오류 0건.
* Windows x86 **Release 전체 빌드**: 오류 0건.
* 단위 테스트: `checks: 1727, failures: 0`.
* CTest 6개 중 5개 통과. 실패한 `re2dj_windows_vfs_runtime_probe`는 **기존 실패**다. 변경을 stash하고 같은 baseline을 다시 빌드해 실행했을 때 동일하게 `audio trace omitted streaming wrap refresh (error 0)`로 실패했다. 오디오 스트리밍 검증이며 이 작업 범위와 무관하다.
* 실행 검증: `ez2dj3rd` 타이틀 화면에서 세 정책을 각각 실행했다.

| 정책 | trace 기록 | 타이틀바 FPS |
| --- | --- | --- |
| 기본(옵션 없음) | `requested=0:applied_interval=1` | 56.0~56.8 |
| `--vsync off` | `requested=1:applied_interval=0` | 56.0~57.4 |
| `--vsync adaptive` | `requested=2:applied_interval=-1` | 55.8~56.3 |

기본 정책이 오늘과 같은 56~57 fps를 냈으므로 **무변경**이 확인됐다. 잘못된 값(`--vsync bogus`)은 거부된다.

### 원인 조사 결과 — 가설 기각

이 기능으로 원래 목적인 원인 규명을 끝냈고, 결과는 **이전에 세운 가설을 기각한다.**

조사 초기에 확인한 사실은 그대로다. 호스트는 60.000 Hz이고, 백엔드와 같은 절차의 독립 probe는 swap interval 1에서 정확히 60.00 fps를 냈으며, 게스트 프로세스 CPU는 한 코어의 15.6~16.4%이고, 창 모드와 전체화면 수치가 같았다. 여기서 "게스트 자체 페이싱과 vsync가 직렬로 걸려 5~6%의 프레임이 밀린다"는 이중 페이싱 가설을 세웠다.

**그 가설은 틀렸다.** vsync를 완전히 끄고(`applied_interval=0`) 실행해도 `ez2dj3rd`는 56~57 fps 그대로다. adaptive도 마찬가지다. present가 전혀 블록하지 않는데도 속도가 같다면, 상한은 present 경계에 없다. **게스트 3rd 자신이 약 17.6 ms 주기로 페이싱한다.**

따라서 프레임률을 60으로 올리려면 present 정책이 아니라 게스트의 페이싱 루프가 왜 16.67 ms가 아닌 17.6 ms를 만드는지를 봐야 한다. 이는 별도 작업이다.

### 범위에서 뺀 것

* 3rd의 56~57 fps 수정. 위 결과로 조사 방향이 바뀌었으며 후속 작업이다.
* Linux·Web 호스트의 정책 실행 확인. 코어 필드는 공용이지만 두 호스트에서 실행하지 않았다.
* 프로파일별 비기본 정책 지정. 어떤 프로파일도 vsync 외의 값을 쓰지 않는다.
* 프레임 시간 히스토그램 계측. 3rd는 `Blt`로 present하므로 `Flip` 안에만 있는 `FrameDraws` 요약이 남지 않는다는 사실을 조사 중 확인했다.

### 환경 문제

CTest가 남긴 `re2dj_windows_vfs_runtime_probe`(PID 41628)와 이전 실행의 `EZ2DJ`(PID 20944)가 종료되지 않는 상태로 남아 빌드 산출물과 CHD staging 파일을 잠갔다. 샌드박스에서 이 둘을 종료할 수 없어, 잠긴 파일을 rename으로 비켜 두고 빌드와 실행을 진행했다. 두 프로세스는 아직 남아 있으므로 사용자가 정리해야 한다.

## English

### Background

This started from the report that `ez2dj3rd` runs at 56-57 fps. While investigating, the repository turned out to **never call `SDL_GL_SetSwapInterval`**, so its vsync behavior was the driver default rather than a decision. This task makes the policy explicit.

### Implementation

| Layer | Change |
| --- | --- |
| Shared core | `PresentSync` plus `PresentSyncName`/`ParsePresentSyncName` in `include/re2dj/graphics/present_sync.h` and `src/graphics/present_sync.cpp` |
| Backend | `Sdl3OpenGlWindowConfig::present_sync`, the interval set in `Initialize`, and the `applied_swap_interval()` accessor |
| Windows global | The `g_re2dj_present_sync` export and `SelectedPresentSync()` |
| Facade | Passes the policy into the window config and writes the applied value to the graphics trace |
| Profile | `TargetRunDefaults::present_sync`, defaulting to vsync |
| Argument builder | Emits `--present-sync` only for a non-default policy |
| Launcher | Parses `--present-sync`, writes the global, forwards it to a followed child, and records it in the launch diagnostic |
| Product CLI | `--vsync on\|off\|adaptive`, its usage text, and the `_explicit` override |
| Tests | `tests/unit/present_sync_test.cpp` |

### Adjustments to the design

The design said the backend would write the applied value to the trace itself. The implementation does not. The shared core has no logging facility, and calling the Windows-only `graphics_trace_log` from the core would break its platform neutrality. The backend exposes the value through `applied_swap_interval()` and the host — the facade — records it. That section of the design was corrected to match.

The design also placed `PresentSync` in `sdl3_opengl_backend.h`, which would have made `target_profile.h` depend on the whole graphics backend interface. It moved to its own `present_sync.h`.

### Verification

* Windows x86 **full Debug build**: no compile or link errors.
* Windows x86 **full Release build**: no errors.
* Unit tests: `checks: 1727, failures: 0`.
* CTest: 5 of 6 pass. The failing `re2dj_windows_vfs_runtime_probe` is a **pre-existing failure**: stashing the changes and rebuilding the same baseline reproduced the identical `audio trace omitted streaming wrap refresh (error 0)`. It is an audio-streaming assertion unrelated to this scope.
* Runtime check: `ez2dj3rd` on its title screen under each policy.

| Policy | Trace record | Title-bar FPS |
| --- | --- | --- |
| Default (no option) | `requested=0:applied_interval=1` | 56.0-56.8 |
| `--vsync off` | `requested=1:applied_interval=0` | 56.0-57.4 |
| `--vsync adaptive` | `requested=2:applied_interval=-1` | 55.8-56.3 |

The default policy reproducing today's 56-57 fps is what confirms **no behavior change**. An invalid value (`--vsync bogus`) is rejected.

### Diagnosis result — the hypothesis is rejected

The feature finished the diagnosis it was built for, and the result **rejects the hypothesis formed earlier.**

The facts measured during the investigation stand: the host runs at 60.000 Hz; a standalone probe built the same way as the backend reached exactly 60.00 fps at interval 1; the guest process used 15.6-16.4% of one core; and windowed and fullscreen agreed. From those, the working hypothesis was double pacing — the guest's own limiter in series with a blocking vsync swap, slipping 5-6% of frames.

**That hypothesis is wrong.** With vsync fully off (`applied_interval=0`), `ez2dj3rd` still runs at 56-57 fps, and adaptive behaves the same. If the rate is unchanged when presents never block, the cap is not at the present boundary. **The 3rd guest paces itself at roughly 17.6 ms.**

Raising the frame rate to 60 therefore means looking at why the guest's pacing loop produces 17.6 ms instead of 16.67 ms, not at the present policy. That is separate work.

### Excluded from scope

* Fixing the 56-57 fps of 3rd. The result above redirects that investigation, which is a follow-up task.
* Runtime confirmation of the policy on the Linux and Web hosts. The core field is shared, but neither host was run.
* Per-profile non-default policies. No profile selects anything but vsync.
* Frame-time histogram instrumentation. The investigation established that 3rd presents through `Blt`, so the `FrameDraws` summary that lives only inside `Flip` never records for it.

### Environment problems

A `re2dj_windows_vfs_runtime_probe` left behind by CTest (PID 41628) and an `EZ2DJ` from an earlier run (PID 20944) stayed alive and locked build outputs and the CHD staging file. Neither could be terminated from this sandbox, so the locked files were renamed aside to let the build and the runs proceed. Both processes are still present and need the user to clear them.
