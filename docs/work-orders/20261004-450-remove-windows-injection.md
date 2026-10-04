# 작업 450 작업 지시서 — Windows 주입 경로 제거 / Task 450 work order — removing the Windows injection path

설계: [작업 446 설계의 5단계](../design/20261004-446-windows-in-process-loader.md)

## 절차 / Steps

1. 주입 런타임·COM facade·`original_process_backend`·Windows 키보드 입력 클래스와 그 테스트, Windows 도구 네 개(`windows_original_process_probe`, `windows_product_loader_probe`, `windows_vfs_runtime_probe`, `windows_x86_launcher_probe`)를 지운다.
   *Delete the injected runtime, COM facades, `original_process_backend`, the Windows keyboard input classes with their tests, and the four Windows tools.*
2. CMake의 주입 블록을 지우고, 오디오 연결과 오디오 테스트를 두 OS 공용 정의 하나로 합친다.
   *Delete CMake's injection block and merge the audio wiring and audio test into one definition for both OSes.*
3. CLI에서 `--demo-volume`, `--audio-volume-trace`, `--guest-wait-trace`, `--vsync`를 지우고, 주입 경로만 읽던 프로파일 필드(`demo_volume`, `hle_*` 일곱 개, `follow_child_process`, `run_detached`, `present_sync`, `guest_wait_trace`, `image_dump`, `image_dump_delay_ms`)와 그 대입·검사를 지운다.
   *Remove the four options from the CLI and the profile fields only injection read, with their assignments and checks.*
4. 릴리스 패키지(`re2dj.exe` 하나), 릴리스 workflow의 VFS probe 단계, 오래된 helper probe 스크립트.
   *The release package (re2dj.exe alone), the release workflow's VFS probe steps, the stale helper probe script.*
5. 문서: README, Windows 런타임 가이드, Windows 플랫폼 README, 스크립트 README, 가이드 색인, AGENTS.md, 헌장, ARCHITECTURE, game-state-hunt 스킬. 작업 로그.
   *Documents and the work log.*
6. 검증: Windows·Linux 두 폭 build와 CTest, Windows 실게임 실행.
   *Verification: builds and CTest on Windows and both Linux widths, and a real Windows run.*
