# #4 작업 지시서 — Windows와 Linux x86·x64 성능 비교 / #4 work order — comparing Windows with Linux x86 and x64 performance

이슈: [#4](https://github.com/reexec/re2DJ/issues/4) · 설계: [20261005-i004-linux-windows-performance.md](../design/20261005-i004-linux-windows-performance.md)

## 절차 / Steps

1. `src/platform/sdl/host_presentation.cpp`에 임시 측정 패치(`RE2DJ_BENCH_VSYNC_OFF`, `RE2DJ_BENCH_FPS_LOG`)를 넣고 x86·x64 Release를 빌드한다. 커밋하지 않는다.
   *Add the temporary measuring patch to `src/platform/sdl/host_presentation.cpp` and build x86 and x64 Release; not committed.*
2. scratchpad의 측정 스크립트로 다섯 타깃 × vsync on/off × x86·x64 × 2회를 잰다.
   *Measure five targets × vsync on/off × x86 and x64 × 2 runs with a scratchpad script.*
3. 패치를 되돌리고 다시 빌드한다.
   *Revert the patch and rebuild.*
4. `docs/analysis/linux-windows-performance.md`를 쓰고 `docs/analysis/README.md` 색인에 넣는다. Windows 문서에서도 링크한다.
   *Write `docs/analysis/linux-windows-performance.md`, index it in `docs/analysis/README.md`, and link it from the Windows document.*

## 완료 조건 / Done when

분석 문서에 Windows·Linux x86·Linux x64의 vsync off 처리량, vsync on CPU·FPS, 메모리 표와 확인됨·추정·미확정 해석이 있다.

*The analysis holds tables of vsync-off throughput, vsync-on CPU and FPS, and memory for Windows, Linux x86 and Linux x64, with confirmed, inferred and unresolved interpretation.*
