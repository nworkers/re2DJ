# TODO

현재 진행 중인 작업과 아직 결정되지 않은 항목만 기록합니다. 완료 항목은 [구현 완료 항목](IMPLEMENTED.md)으로 이동합니다.

*This file contains only active work and unresolved items. Completed items are moved to [Implemented](IMPLEMENTED.md).*

## 현재 진행 / In progress

- [ ] Windows x86 VFS runtime 연결
  - [ ] canonical 실행에서 read/write/close와 overlay 결과를 검증
- [ ] 보호 해제 후 원본 `.text` 초기화 안정화
  - [ ] 640×480×16 surface와 최소 fixed-function draw/present를 OpenGL backend로 연결
  - [ ] `0x0042325c`에서 확인된 `IDirect3DDevice3::DrawPrimitive` HLE와 OpenGL backend 연결

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
