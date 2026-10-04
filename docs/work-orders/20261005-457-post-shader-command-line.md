# 작업 457 작업 지시서 — 셰이더 명령행 옵션을 rePIU와 맞추기 / Task 457 work order — matching rePIU's shader command-line option

설계: [20261005-457-post-shader-command-line.md](../design/20261005-457-post-shader-command-line.md)

## 절차 / Steps

1. `src/host/cli/main.cpp`: `--post-shader=<id>`, `--`(옵션 끝, 뒤는 프로파일 id), 위치 인자 처리를 `TakePositionalTarget`으로 묶기, 자식 인자에서 `--` 빼기, 사용법 두 줄. / *`--post-shader=<id>`, `--` (end of options, the profile id after it), the positional handling as `TakePositionalTarget`, `--` dropped from the child's arguments, and two usage lines.*
2. README 옵션 목록과 셰이더 가이드. / *The README option list and the shader guide.*
3. 검증: 설계의 실제 실행 경우, Windows x86 Debug와 WSL Linux x64(clang) 빌드·CTest. / *Verification: the design's real-run cases, the Windows x86 Debug and WSL Linux x64 (clang) builds and CTest.*

## 완료 조건 / Done when

설계 표의 규칙이 실제 실행에서 모두 확인된다. / *Every rule in the design's table is confirmed by a real run.*
