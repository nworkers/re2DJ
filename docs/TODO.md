# TODO

현재 진행 중인 작업과 아직 결정되지 않은 항목만 기록합니다. 완료 항목은 [구현 완료 항목](IMPLEMENTED.md)으로 이동합니다.

*This file contains only active work and unresolved items. Completed items are moved to [Implemented](IMPLEMENTED.md).*

## 현재 진행 / In progress

- [ ] Windows x86 VFS runtime 연결
  - [x] `C:\\windows` / `D:\\ez2dj` root mapping
  - [x] CWD 기준 `overlays/<target-id>` write policy
  - [x] 공용 `VfsFileTable` (`Open`, `Read`, `Write`, `Seek`, `Size`, `Close`)
  - [x] runtime file API wrapper build (`CreateFileA` ~ `CloseHandle`)
  - [x] launcher root configuration export 전달
  - [x] original IAT에 file API wrapper 연결
  - [x] synthetic VFS read/copy-on-write/close 검증
  - [x] development `ez2dj1.exe` entry에서 첫 `CreateFileA` wrapper 관찰
  - [x] canonical protected `ez2dj.exe`의 post-entry illegal instruction caller 확정 — guest의 직접 branch가 아니라 `FreeLibrary` 유발 DLL 언로드 종반의 WOW64 win32k 시스템 콜 전환 직후 (`--api-trace`, 2026-08-23)
  - [ ] invalid target 조건 분석 — 64비트 전환이 private RW page에 도달하는 경로와, 종료가 하드웨어 동글·환경 검사 실패인지 현대 WOW64 부정합인지 판별
  - [ ] canonical 실행에서 read/write/close와 overlay 결과를 검증

## 다음 작업 / Next work

- [ ] Windows x86 INI API HLE (`GetPrivateProfile*`, `WritePrivateProfileStringA`)
- [ ] Windows x86 directory enumeration (`FindFirstFileA`, `FindNextFileA`, `FindClose`)
- [ ] Web(Emscripten) build verification
- [ ] GitHub Actions first workflow verification

## 분석 미완료 / Analysis remaining

- [ ] `Songs/` 아래 자산 파일 형식 분석
- [ ] 3rd target guest drive and working-directory evidence
- [ ] `syscall_resume_hit`에서 stack 상단 64 word dump로 `RtlDestroyHeap` 소유 확인(ws2_32 detach 여부)과 힙 핸들–fault allocation 상관
- [ ] fault 시점 memory에서 `{entry VA ×4}` 식재 패턴을 찾아 안정적 fault 서명(ECX~EDI=entry)의 공급원 추적
- [ ] 두 번째 게이트의 64비트 처리가 private RW page 전송을 만드는 메커니즘과, 종료가 동글·환경 검사 실패인지 현대 WOW64 부정합인지 판별
- [ ] HLE 관점 완화 검토 — 보호 stub의 `FreeLibrary("WSOCK32.DLL")` 후킹이나 Winsock probe 대체가 현대 환경 종료를 피할 수 있는지
- [ ] 3rd 보호 빌드(`EZ2DJ.EXE`)의 import 표면과 런타임 흐름

## 보류 / Deferred

- [ ] Windows x64 host expansion
- [ ] Custom x86-32 interpreter, only if no permitted Web execution engine is suitable
