# #4 설계 — Windows와 Linux x86·x64 성능 비교 / #4 design — comparing Windows with Linux x86 and x64 performance

이슈: [#4](https://github.com/reexec/re2DJ/issues/4) · 기준: [Windows in-process 러너 성능](../analysis/windows-in-process-performance.md)(작업 453)

## 목적 / Goal

작업 453의 Windows x86 in-process 측정과 같은 PC·같은 조건으로 Linux x86·x64 Release를 재서 비교한다. 결과와 해석은 `docs/analysis/linux-windows-performance.md`에 둔다. 코드는 바꾸지 않는다.

*Measure the Linux x86 and x64 Release builds on the same PC under task 453's conditions for the Windows x86 in-process runner, and record the comparison in `docs/analysis/linux-windows-performance.md`; no code changes.*

## 조건 / Conditions

| 항목 | Windows(작업 453) | Linux(이 작업) |
| --- | --- | --- |
| PC | Ryzen 5 5600X, RTX 4090, 3840x2160 60 Hz, Windows 11 Pro 26200 | 같은 CPU·GPU(`lscpu`, `glxinfo`), 5120x2880 59.99 Hz ×2, Ubuntu 26.04, GNOME Wayland, NVIDIA 595 |
| 빌드 | Release x86(MSVC) | `linux-x86-release`, `linux-x64-release`(GCC 15.2), 이 브랜치의 HEAD |
| 표시 | SDL3 OpenGL(WGL) | SDL3 OpenGL(네이티브 Wayland EGL). x86은 NVIDIA 32비트 `egl-wayland2` 문제로 `egl-wayland`(v1)를 지정한다(작업 459 가이드) |
| 실행 | 타깃별 vsync on/off 40초, 2배 창, 입력 없음 | 같음. 조건마다 2회, x86·x64를 번갈아 |
| vsync off | 임시 패치 `RE2DJ_BENCH_VSYNC_OFF` → `PresentSync::kImmediate` | 같은 임시 패치 |
| FPS | 창 제목(1초 단위 `FrameRate`) | 같은 `FrameRate` 값을 임시 패치로 로그에 기록(`RE2DJ_BENCH_FPS_LOG`). Wayland에서는 다른 프로세스가 창 제목을 읽을 수 없다 |
| CPU | 프로세스 트리 누적 CPU 시간(1초마다) | `/proc/<pid>/stat`의 utime+stime, 프로세스 트리(6th 자식 포함) |
| 메모리 | Working set, Private bytes | RSS(working set에 해당), `smaps_rollup`의 Private_Clean+Private_Dirty(상주 private), `VmData`(commit된 private 주소 공간에 가까움) |
| 집계 | 15초 이후 | 같음 |

*The table maps each of task 453's conditions to Linux: the same CPU and GPU, two 5120x2880 59.99 Hz displays, Ubuntu 26.04 with GNOME Wayland and NVIDIA 595; `linux-x86-release` and `linux-x64-release` (GCC 15.2) from this branch's HEAD; SDL3 OpenGL on native Wayland EGL, x86 pinning `egl-wayland` v1 for NVIDIA's 32-bit `egl-wayland2` (the task 459 guide); 40 s per target with vsync on and off in the 2x window with no input, twice per condition, alternating x86 and x64; the same temporary patch for vsync off; the same `FrameRate` value written to the log by a temporary patch (`RE2DJ_BENCH_FPS_LOG`), since another process cannot read a window title under Wayland; CPU from utime+stime in `/proc/<pid>/stat` across the process tree (6th's child included); RSS for the working set, Private_Clean+Private_Dirty from `smaps_rollup` for resident private memory, and `VmData` as the nearest to committed private address space; aggregation after 15 s.*

## 위험 / Risks

- GNOME은 가려진 창의 프레임을 1 Hz로 줄인다(작업 458). vsync on 측정이 영향을 받으므로, FPS가 2 미만인 초가 있으면 그 실행을 버리고 다시 잰다.
- 측정 패치는 측정 뒤 되돌리고 다시 빌드한다(작업 453과 같다).

*GNOME cuts a covered window's frames to 1 Hz (task 458), which would distort vsync-on runs, so a run with any second below 2 fps is discarded and repeated. The measuring patch is reverted and the builds rebuilt afterwards, as in task 453.*
