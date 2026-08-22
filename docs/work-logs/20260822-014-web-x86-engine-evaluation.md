# 작업 로그: Web x86 실행 엔진 평가

## 결과

Web에서 원본 IA-32 코드를 실행할 수 있는 후보와 라이선스를 공식 자료 기준으로 비교했다. 서드파티 코드는 도입하지 않았다.

- v86: BSD-2-Clause, WebAssembly JIT와 IA-32/FPU/SSE 범위가 확인되었다. 완전한 PC 구조에서 CPU 경계를 분리할 수 있는지는 미확정이므로 제한된 spike의 1순위로 정했다.
- TinyEMU/JSLinux: TinyEMU는 MIT지만 현재 Web x86 구현의 재사용 가능한 공개 소스 경계가 확인되지 않아 보류했다.
- libx86emu: 허용형 라이선스와 hook API는 적합하지만 FPU/MMX/SSE 부재로 범위가 부족하다.
- Blink: ISC지만 x86-64 Linux 사용자 공간 중심이고 공식 Web 호스트 근거가 없어 제외했다.
- QEMU, Unicorn, Bochs, Halfix, MAME: GPL/LGPL 범위를 포함해 프로젝트 정책상 제외했다.

Stage 3 완료 기준인 데스크톱 native helper synthetic PE32 gate 검증과 Web 후보·라이선스 문서화가 모두 충족되어 포팅 계획 상태를 완료로 바꿨다. Linux PE32 helper adapter는 플랫폼 확장 작업으로 TODO에 남아 있다.

## 변경 문서

- 평가 기준과 결정: `docs/design/20260822-014-web-x86-engine-evaluation.md`
- 일반 기술 조사: `docs/kb/web-x86-execution-engines.md`
- 진행 상태: `docs/TODO.md`, `docs/WIN32_HLE_PORTING_PLAN.md`
- 현재 구조: `ARCHITECTURE.md`, `src/platform/web/README.md`

## 검증

- 후보별 공식 저장소, 공식 프로젝트 페이지와 라이선스 원문을 대조했다.
- 새 지식 문서를 `docs/kb/README.md` 색인에 추가했다.
- 후보 엔진 소스, 바이너리, 원본 게임 자산이 변경에 포함되지 않았음을 확인했다.
- 코드 변경이 없으므로 빌드·단위 테스트는 실행하지 않았다.

## 다음 작업

전체 v86 PC를 통합하지 않는 별도 spike를 작성한다. 최소 CPU·메모리·실행 루프 분리, synthetic import gate 반환·재개, self-modifying code용 번역 무효화, 실제 최소 소스 집합의 전이 라이선스만 검증한다.

---

# Work Log: Web x86 Execution Engine Evaluation

## Result

Candidates capable of executing original IA-32 code on the Web and their licenses were compared using official sources. No third-party code was introduced.

- v86: BSD-2-Clause with verified WebAssembly JIT and IA-32/FPU/SSE coverage. CPU-boundary separability from its complete-PC architecture remains unresolved, so it is the first bounded-spike candidate.
- TinyEMU/JSLinux: TinyEMU is MIT, but the reusable published-source boundary of the current Web x86 implementation is unconfirmed, so it was deferred.
- libx86emu: Its permissive license and hook APIs fit, but missing FPU/MMX/SSE support leaves insufficient coverage.
- Blink: ISC, but excluded because it focuses on x86-64 Linux userspace and has no official Web host evidence.
- QEMU, Unicorn, Bochs, Halfix, and MAME: Excluded under project policy due to GPL/LGPL scope.

The Stage 3 completion criteria—synthetic PE32 gate validation through a desktop native helper plus documented Web candidates and licenses—are now satisfied, so the porting-plan status was changed to complete. The Linux PE32 helper adapter remains a platform-extension TODO.

## Changed Documentation

- Evaluation criteria and decision: `docs/design/20260822-014-web-x86-engine-evaluation.md`
- General technical survey: `docs/kb/web-x86-execution-engines.md`
- Progress state: `docs/TODO.md`, `docs/WIN32_HLE_PORTING_PLAN.md`
- Current structure: `ARCHITECTURE.md`, `src/platform/web/README.md`

## Verification

- Cross-checked official repositories, project pages, and original license texts for each candidate.
- Added the new knowledge topic to the `docs/kb/README.md` index.
- Confirmed that no candidate-engine source, binary, or original game asset is included in the changes.
- No build or unit tests were run because there were no code changes.

## Next Task

Write a separate spike that does not integrate the complete v86 PC. It will validate only minimum CPU/memory/execution-loop separation, synthetic import-gate return and resume, translation invalidation for self-modifying code, and transitive licenses of the actual minimum source set.
