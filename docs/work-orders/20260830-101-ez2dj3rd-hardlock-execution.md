# ez2dj3rd Hardlock 실행 작업 지시

## 목적

사용자가 제공한 3rd HDD에서 `re2dj ez2dj3rd`를 실행하고, 1st SE LPTDI 정책을 재사용하지 않는 상태로 실제 다음 실패 경계를 확인한다.

*Run `re2dj ez2dj3rd` against the user-provided 3rd HDD and identify the next real failure boundary without reusing the 1st SE LPTDI policy.*

## 작업 범위

1. 관련 설계 문서를 작성한다.
2. VFS runtime에 `\\.\\` 장치 경로의 bounded trace를 추가한다.
3. 3rd 프로파일로 Windows x86 Debug 실행을 수행한다.
4. 실행 로그와 창 상태를 확인하고, 실제로 확인된 경계만 문서화한다.
5. Hardlock 응답 계약이 확인되지 않으면 mock/IOCTL/raw I/O를 추측해 구현하지 않는다.

*Scope: write the design, add a bounded `\\.\\` device trace to the VFS runtime, run the Windows x86 Debug 3rd profile, inspect logs and window state, document only confirmed boundaries, and avoid guessing a Hardlock mock, IOCTL response, or raw-I/O behavior.*

## 완료 기준

- 3rd 프로파일이 `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE`를 선택한다.
- runtime 주입, VFS mount, DirectSound hook, detached 실행이 확인된다.
- 원본 프로세스의 다음 경계가 기록된다.
- 1st SE의 LPTDI target state와 IOCTL 응답이 3rd에 연결되지 않는다.
- Windows x86 Debug build와 unit test가 통과한다.

*Completion criteria: the profile selects `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE`; runtime injection, VFS mount, DirectSound hook, and detached execution are confirmed; the next boundary of the original process is recorded; no 1st SE LPTDI state or IOCTL response is connected to 3rd; and the Windows x86 Debug build and unit tests pass.*

## 결과

**완료 — Hardlock 실패 경계까지.** 제품 실행은 `EZ2DJ.EXE`를 응답 상태로 유지하고 `Hardlock` 대화상자에서 멈춘다. 대화상자 이후의 실제 보호 응답 계약은 미확정이므로 3rd 전용 mock은 추가하지 않았다.

검증 로그: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-211620-710.jsonl`

*Completed through the Hardlock failure boundary. The product run keeps `EZ2DJ.EXE` responsive and stops at the `Hardlock` dialog. No 3rd-only mock was added because the protection response contract after the dialog is unresolved.*

*Verification log: `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-211620-710.jsonl`*
