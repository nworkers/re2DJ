# TODO

현재 진행 중인 작업과 아직 결정되지 않은 항목만 기록합니다. 완료 항목은 [구현 완료 항목](IMPLEMENTED.md)으로 이동합니다.

*This file contains only active work and unresolved items. Completed items are moved to [Implemented](IMPLEMENTED.md).*

## 현재 진행 / In progress

- [ ] 작업 072 — 메인 루프 사용자 검증
  - [ ] OpenGL 첫 프레임과 texture-stage state의 시각 정확성을 확인
  - [ ] `title.wav`, 효과음, volume/pan/frequency와 duplicate 동시 재생을 청취 확인
  - [ ] 키보드와 legacy I/O port 입력으로 메뉴·게임 상태 전이가 가능한지 확인

  *Task 072 — Validate the main loop from the user's perspective: first-frame and texture-stage visuals, title/effect audio and duplicate playback, and keyboard/legacy-I/O navigation.*

- [ ] Windows x86 VFS guest write/overlay 검증
  - [ ] canonical 실행에서 guest write가 원본 HDD가 아닌 overlay에 기록되는지 확인

  *Verify that canonical guest writes go to the overlay rather than modifying the original HDD directory. The read/seek/size/close path already runs through the original main-loop startup.*

## 다음 작업 / Next work

- [ ] Windows x86 INI API HLE (`GetPrivateProfile*`, `WritePrivateProfileStringA`)
- [ ] Windows x86 directory enumeration (`FindFirstFileA`, `FindNextFileA`, `FindClose`)
- [ ] Web(Emscripten) build verification
- [ ] GitHub Actions first workflow verification

## 분석 미완료 / Analysis remaining

- [ ] `Songs/` 아래 자산 파일 형식 분석
- [ ] 3rd target guest drive and working-directory evidence
- [ ] 실패 경로 continuation buffer(힙 페이지)의 의미 추적 — entry 직후 XOR 루프 대상 버퍼와 `[0x01ed7074]` 플래그 기입 지점
- [ ] 3rd 보호 빌드(`EZ2DJ.EXE`)의 import 표면과 런타임 흐름

## 보류 / Deferred

- [ ] Windows x64 host expansion
- [ ] Custom x86-32 interpreter, only if no permitted Web execution engine is suitable
