# ez2dj6th VFS GetFullPathNameA HLE 및 그래픽 에셋 로드 정상화 작업 지시서
# Work Order: ez2dj6th VFS GetFullPathNameA HLE and Graphic Asset Loading Normalization

## 1. 개요 (Overview)

`ez2dj6th` 실행 시 화면에 정상 그래픽 대신 파일명 텍스트(`6logo`, `ttr base` 등)가 렌더링되는 문제의 원인인 `GetFullPathNameA` 가상 현재 디렉터리 미반영 문제를 해결하고, VFS HLE 계층에 `GetFullPathNameA` thunk를 추가하여 그래픽 비트맵 에셋이 정상 로드되도록 구현한다.

---

Fix the issue where placeholder filename text (`6logo`, `ttr base`, etc.) is rendered instead of actual graphics in `ez2dj6th` by implementing `GetFullPathNameA` HLE in the VFS layer, ensuring relative paths are resolved against the virtual guest current directory.

---

## 2. 작업 목표 (Objectives)

1. `src/platform/windows/injected_runtime.cpp`:
   - `Re2djVfsGetFullPathNameA` 구현 및 dllexport.
   - `Re2djHleGetProcAddress`에 `"GetFullPathNameA"` 매핑 추가.
2. `src/tools/windows_x86_launcher_probe/child_process_handoff.cpp` & `main.cpp`:
   - `vfs_exports` / `vfs_imports` 목록에 `GetFullPathNameA` (`_Re2djVfsGetFullPathNameA@16`) 추가.
3. `src/host/cli/main.cpp`:
   - `PrepareChdStaging`에서 `ez2dj6th`의 `EZ2DJ/EZ2DJ6th.EXE` 자동 머티리얼라이즈 보장.
4. `src/tools/windows_vfs_runtime_probe/main.cpp`:
   - `Re2djVfsGetFullPathNameA` 단위 테스트 추가.
5. 빌드 및 실기 검증 수행.

---

## 3. 세부 작업 항목 (Tasks)

- [ ] `src/platform/windows/injected_runtime.cpp`에 `Re2djVfsGetFullPathNameA` 추가 및 `GetProcAddress` 연동
- [ ] `src/tools/windows_x86_launcher_probe/child_process_handoff.cpp` 및 `main.cpp` IAT 패치 테이블에 `GetFullPathNameA` 추가
- [ ] `src/host/cli/main.cpp`에서 `ez2dj6th` 자식 바이너리 스테이징 추가
- [ ] `src/tools/windows_vfs_runtime_probe/main.cpp`에 단위 검증 추가
- [ ] 컴파일 및 단위 테스트 실행
- [ ] `re2dj.exe ez2dj6th --io-config .\config\ez2dj-io.example.ini` 실기 실행 검증
- [ ] 작업 로그 문서 작성
