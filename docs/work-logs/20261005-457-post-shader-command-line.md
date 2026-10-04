# 작업 457 작업 로그 — 셰이더 명령행 옵션을 rePIU와 맞추기 / Task 457 work log — matching rePIU's shader command-line option

설계: [20261005-457-post-shader-command-line.md](../design/20261005-457-post-shader-command-line.md) · 지시서: [20261005-457-post-shader-command-line.md](../work-orders/20261005-457-post-shader-command-line.md)

## 2026-10-05

- **확인**: rePIU를 fetch해 v0.0.201(`77117bf`, 작업 771)의 설계를 읽었다. 위치 자유, 명령행 우선, 반복 시 마지막 값, 값 없음 exit 1은 re2DJ에 이미 있었고, `=` 형식과 `--`가 없었다.
- **구현**: `--post-shader=<id>`(빈 값 거부), `--` 뒤 인자를 프로파일 id로 읽음(`TakePositionalTarget`으로 기존 위치 인자 처리와 공유), 자식 인자에서 `--`를 뺌, 사용법 두 줄. README 옵션 목록과 셰이더 가이드 갱신.

  *Check: rePIU was fetched and v0.0.201's design (`77117bf`, task 771) read; free placement, the command line winning, the last of a repeated option and exit 1 on a missing value were already in re2DJ, while the `=` form and `--` were not. Implementation: `--post-shader=<id>` (an empty value refused), arguments after `--` read as the profile id (sharing the positional handling through `TakePositionalTarget`), `--` dropped from the child's arguments, and two usage lines; the README option list and the shader guide updated.*

- **검증**
  - Windows x86 Debug: `re2dj` 빌드 성공(경고 없음). 실제 실행(4th, 6th):

    | 실행 | 결과 |
    | --- | --- |
    | `--post-shader=scanline ez2dj4th` | scanline, 2 parameters |
    | `ez2dj4th --post-shader crt`, `RE2DJ_POST_SHADER=none` | crt(명령행 우선) |
    | `--post-shader=crt -- ez2dj4th` | crt |
    | `ez2dj4th --post-shader` | `--post-shader requires a value`, exit 1 |
    | `ez2dj4th --post-shader=` | `--post-shader needs a shader id, or none`, exit 1 |
    | `ez2dj4th --post-shader crt --post-shader=scanline` | scanline(마지막 값) |
    | `--post-shader=crt -- ez2dj6th` | 런처와 `EZ2DJ6TH.EXE` 자식 모두 crt, 자식 창에 적용(자식 옵션이 `--` 뒤로 가지 않음) |

  - WSL Linux x64 clang(경고를 오류로): 빌드 성공, CTest 5개 통과. `re2dj --post-shader=scanline -- ez2dj4th`는 scanline 적용, `ez2dj4th --post-shader=`는 exit 1.

  *Verification: the Windows x86 Debug `re2dj` builds with no warnings, and real 4th and 6th runs confirm every case in the table above — the `=` form before the target, the command line over `RE2DJ_POST_SHADER=none`, the target after `--`, exit 1 with its message for a missing and an empty value, the last of a repeated option, and 6th started with `--` reaching its `EZ2DJ6TH.EXE` child with crt applied (the child options not landing after `--`). WSL Linux x64 clang (warnings as errors) builds and passes 5 CTest tests; `re2dj --post-shader=scanline -- ez2dj4th` applies scanline and `ez2dj4th --post-shader=` exits 1.*
